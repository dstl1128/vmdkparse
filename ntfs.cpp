//
// NTFS file system parser
// Mainly parses the header of NTFS file system,
// and obtain the data run of the $MFT, and provides
// interface to obtain MFT record through MFT reference number.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "types.h"
#include "ntfs.h"
#include "ntfs_attr.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>

using namespace ntfs;

//=============================================================================
u32 AttributeLength(ATTRIBUTE * p)
{
    return p->Nonresident
        ? (u32) PNONRESIDENT_ATTRIBUTE(p)->RealSize
        : PRESIDENT_ATTRIBUTE(p)->ValueLength;
}

//=============================================================================
void Ntfs::Test()
{

}

//=============================================================================
Ntfs::Ntfs(disk::IDiskRead & disk, int partitionNum)
: _disk(disk), _partitionNum(partitionNum), _bytesPerFileRecord(0), _mftSize(0)
{
    Init();
}

//=============================================================================
void Ntfs::Init()
{
    InitBoot();

    InitMft();

    if (_bootb.BytesPerSector != 512)
        throw std::runtime_error("Bytes per sector is not 512.");

    if ((_bytesPerFileRecord % _bootb.BytesPerSector) != 0)
        throw std::runtime_error("MFT size must be divisable by sector size.");
}

//=============================================================================
void Ntfs::InitBoot()
{
    //printf("BOOT_BLOCK size: %u\n", sizeof(_bootb));

    if (!_disk.ReadSector(0, &_bootb, _partitionNum))
        throw std::runtime_error("Can't read NTFS boot sector.");

    if (_bootb.Format[0] != 'N' || _bootb.Format[1] != 'T' || _bootb.Format[2] != 'F' || _bootb.Format[3] != 'S')
        throw std::runtime_error("This is not NTFS partition.");

    if (_bootb.BytesPerSector != 512)
        throw std::runtime_error("Bytes per sector is not 512.");

    _bytesPerFileRecord = (_bootb.ClustersPerFileRecord < 0x80)
        ? (_bootb.ClustersPerFileRecord * _bootb.SectorsPerCluster * _bootb.BytesPerSector)
        : (1 << (0x100 - _bootb.ClustersPerFileRecord));

    //printf("Format: %8.8s\n", _bootb.Format);
    //printf("Bytes per sector: %u\n", _bootb.BytesPerSector);
    //printf("Sectors per cluster: %u\n", _bootb.SectorsPerCluster);
    //printf("Boot sectors: %u\n", _bootb.BootSectors);
    //printf("Partition offset: %u\n", _bootb.PartitionOffset);
    //printf("Total sector: %I64u\n", _bootb.TotalSectors);
    //printf("MFT start LCN: %I64u\n", _bootb.MftStartLcn);
    //printf("MFT2 start LCN: %I64u\n", _bootb.Mft2StartLcn);
    //printf("Cluster per file record: %u\n", _bootb.ClustersPerFileRecord);
    //printf("Cluster per index blocK: %u\n", _bootb.ClustersPerIndexBlock);
    //printf("Volume serial: %I64x\n", _bootb.VolumeSerialNumber);
    //printf("Boot signature: %x\n", _bootb.BootSignature);
}

//=============================================================================
void Ntfs::InitMft()
{
    u64 startMFTsector = _bootb.MftStartLcn * _bootb.SectorsPerCluster;

    // reads $MFT file record
    _mft.resize(_bytesPerFileRecord);
    if (!_disk.ReadSectorN(startMFTsector, _bytesPerFileRecord / _bootb.BytesPerSector, &_mft[0], _partitionNum))
        throw std::runtime_error("Can't read MFT.");
    //dump("mft.bin", &_mft[0], _mft.size());
    if (!ApplyUpdateSequence(&_mft[0], _mft.size()))
        throw std::runtime_error("Error applying update sequence.");
    if (_mft[0] != 'F' || _mft[1] != 'I' || _mft[2] != 'L' || _mft[3] != 'E')
        throw std::runtime_error("First MFT record must be $MFT file record.");
    FILE_RECORD_HEADER * phdr = (FILE_RECORD_HEADER*)&_mft[0];
    if (!(phdr->Flags & 0x01))
        throw std::runtime_error("WTF!!! MFT not in used?!");

    // read $MFT attributes
    for (ATTRIBUTE * pattr = (ATTRIBUTE*)P_add(&_mft[0], phdr->AttributesOffset);
            pattr < (ATTRIBUTE*)P_add(&_mft[0], _mft.size()) && pattr->AttributeType != eAttributeTerminator;
            pattr = (ATTRIBUTE*)P_add(pattr, pattr->Length))
    {
        switch (pattr->AttributeType)
        {
        case eAttributeData:
            {
                if (_pAttrData.get() != 0)
                    throw std::runtime_error("MFT having two data attr.");
                _pAttrData.reset(new ntfs::AttributeData);
                _pAttrData->Init((u8*)pattr, pattr->Length);
                //dump("mft_attr_data.bin", &_pAttrData->_data[0], _pAttrData->_data.size());
                //_pAttrData->Print();
                _mftSize = _pAttrData->_realSize;
                _mftAllocatedSize = _pAttrData->_allocatedSize;
                _mftStartVcn = _pAttrData->_startVcn;
                _mftEndVcn = _pAttrData->_endVcn;
            }
            break;
        case eAttributeAttributeList:
            {
                if (_pAttrList.get() != 0)
                    throw std::runtime_error("MFT having two list attr.");
                _pAttrList.reset(new ntfs::AttributeList);
                _pAttrList->Init((u8*)pattr, pattr->Length);
                //dump("mft_attr_list.bin", &_pAttrList->_data[0], _pAttrList->_data.size());
                //_pAttrList->Print();
            }
            break;
        default:
            {
                //ntfs::Attribute attr;
                //attr.Init((u8*)pattr, pattr->Length);
                //attr.Print();
            }
            break;
        }
    }

    // init data runs
    _pMftDataRun.reset(new DataRun);

    // init MFT from single $DATA attr
    if (_pAttrData.get() == 0)
        throw std::runtime_error("No MFT data.");
    if (!_pAttrData->_nonResident)
        throw std::runtime_error("MFT should never be stored as resident struct.");
    _pMftDataRun->Init((u8*)&_pAttrData->_data[0], _pAttrData->_data.size(), _pAttrData->_startVcn);

    // init MFT from multiple location -> mega-fragmented!
    if (_pAttrList.get() != 0)
    {
        if (_pAttrList->_nonResident)
            throw std::runtime_error("Non-resident attr list not supported.");
        if (_pAttrList->_data.empty())
            throw std::runtime_error("Need attribute list for resolving $MFT multi-dataruns.");

        std::vector<u8> buf(_bytesPerFileRecord);
        FILE_RECORD_HEADER * phdr = (FILE_RECORD_HEADER*)&buf[0];
        ATTRIBUTE_LIST * pListEntry = (ATTRIBUTE_LIST*)&_pAttrList->_data[0];
        ATTRIBUTE_LIST * pListEntryEnd = P_add(pListEntry, _pAttrList->_data.size());
        for (; pListEntry < pListEntryEnd && pListEntry->Length > 0; pListEntry = P_add(pListEntry, pListEntry->Length))
        {
            if (!(pListEntry->FileReferenceNumber & MFT_MASK))  // skip if is $MFT entry (the 1st entry in $MFT)
                continue;

            ReadFileRecord(pListEntry->FileReferenceNumber, phdr);
            if (buf[0] != 'F' || buf[1] != 'I' || buf[2] != 'L' || buf[3] != 'E')   // skip non file record
                continue;
            if (!(phdr->Flags & 0x01)) // skip not in used entry
                continue;

            for (ATTRIBUTE * pattr = (ATTRIBUTE*)P_add(phdr, phdr->AttributesOffset);
                    pattr < (ATTRIBUTE*)P_add(&buf[0], buf.size()) && pattr->AttributeType != eAttributeTerminator;
                    pattr = P_add(pattr, pattr->Length))
            {
                if (pattr->AttributeType == eAttributeData)
                {
                    _pAttrData->Clear();
                    _pAttrData->Init((u8*)pattr, pattr->Length);
                    if (pListEntry->AttributeNumber == _pAttrData->_attrId && pListEntry->AttributeType == _pAttrData->_attrType)
                        _pMftDataRun->Append(&_pAttrData->_data[0], _pAttrData->_data.size(), _pAttrData->_startVcn);
                    //_pAttrData->Print();
                }
                else
                {
                    // just printing out unused attr found
                    //ntfs::Attribute attr;
                    //attr.Init((u8*)pattr, pattr->Length);
                    //attr.Print();
                }
            } // loop each attributes in file record
        } // loop each list entry (each list entry refer to a real file record)
    } // if attr list available
}

//=============================================================================
bool Ntfs::ApplyUpdateSequence(void * buf, u32 /*bufSize*/)
{
    u8 * bytes = (u8*)buf;
    if (bytes[0] == 'F' && bytes[1] == 'I' && bytes[2] == 'L' && bytes[3] == 'E')
    {
        FILE_RECORD_HEADER * prec = (FILE_RECORD_HEADER*)buf;
        u32   usaOffset       = prec->Ntfs.UsaOffset;
        u32   usaCount        = prec->Ntfs.UsaCount - 1;
        u16 *  usaPos          = (u16*)buf + usaOffset/sizeof(u16);
        u16    usaChecksum     = *usaPos;
        u16 *  sectorLastWord  = (u16*)buf + (_bootb.BytesPerSector/sizeof(u16) - 1);
        u32   usaSize = 0;

        if (prec->BytesAllocated < (usaCount * _bootb.BytesPerSector))
            return false;

        while (usaCount --> 0)
        {
            if (*sectorLastWord != usaChecksum)
            {
                // ignore bad update sequence if entry not affected by it.
                if (usaSize <= prec->BytesInUse)
                    return false;
                break;
            }
            *sectorLastWord = *(++usaPos);

            sectorLastWord += _bootb.BytesPerSector/sizeof(u16);
            usaSize += _bootb.BytesPerSector;
        }
        return true;
    }
    return false;
}


//=============================================================================
void Ntfs::ReadLCN(u64 lcn, u32 count, void * buf)
{
    if (!_disk.ReadSectorN(lcn * _bootb.SectorsPerCluster, count * _bootb.SectorsPerCluster, buf, _partitionNum))
        throw std::runtime_error("Error reading LCN cluster.");
}

//=============================================================================
ATTRIBUTE * Ntfs::FindAttribute(FILE_RECORD_HEADER * phdr, ATTRIBUTE_TYPE type, u16 const * name)
{
    //u8 * bytes = (u8*)phdr + phdr->AttributesOffset;
    for (ATTRIBUTE * pattr = (ATTRIBUTE*)P_add(phdr, phdr->AttributesOffset);
            pattr->AttributeType != eAttributeTerminator;
            pattr = P_add(pattr, pattr->Length))
    {
        if (pattr->AttributeType == type)
        {
            if (name == 0) // && pattr->NameLength == 0)  // doesn't need name compare
                return pattr;
            if (name != 0 && u16len(name) == pattr->NameLength && u16cmp(name, (u16*)P_add(pattr, pattr->NameOffset)) == 0)
                return pattr;
        }
    }
    return 0;
}

//=============================================================================
void Ntfs::ReadFileRecord(u64 index, void * phdr)
{
    if (_pMftDataRun.get() == 0 || _pMftDataRun->_list.empty())
        throw std::runtime_error("Requesting data from $MFT before parsing $MFT info.");
    index = index & MFT_MASK;
    u32 clusters = _bootb.ClustersPerFileRecord;
    if (clusters & 0x80) clusters = 1;
    std::vector<u8> p(_bootb.BytesPerSector * _bootb.SectorsPerCluster * clusters);
    u64 vcn = index * _bytesPerFileRecord / _bootb.BytesPerSector / _bootb.SectorsPerCluster;
    ReadLCN(_pMftDataRun->Vcn2Lcn(vcn), clusters, &p[0]);
    s32 m = (_bootb.SectorsPerCluster * _bootb.BytesPerSector / _bytesPerFileRecord) - 1;
    u32 n = m > 0 ? (index & m) : 0;
    memcpy(phdr, &p[0] + n * _bytesPerFileRecord, _bytesPerFileRecord);
    ApplyUpdateSequence(phdr, _bytesPerFileRecord);
}

