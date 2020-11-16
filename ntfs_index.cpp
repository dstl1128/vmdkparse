//
// NTFS Index
// Parses the IndexAllocation attributes and
// reconstruct the files/folders hierarchy.
// *Currently unused & buggy.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "types.h"
#include "ntfs_index.h"

#include <map>
#include <utility>

using namespace ntfs;

char g_attr[] =
{
    /*       1 */  'R',     // read only
    /*       2 */  'H',     // hidden
    /*       4 */  'S',     // system
    /*       8 */  '3',
    /*      10 */  '4',
    /*      20 */  'A',     // archive
    /*      40 */  '6',
    /*      80 */  'N',     // Win32API is Normal
    /*     100 */  'T',     // temporary
    /*     200 */  '9',
    /*     400 */  'a',
    /*     800 */  'b',     // ** unused?
    /*    1000 */  'O',     // offline
    /*    2000 */  'd',
    /*    4000 */  'E',     // encrypted
    /*    8000 */  'f',
    /*   10000 */  'g',
    /*   20000 */  'h',
    /*   40000 */  'i',
    /*   80000 */  'j',
    /*  100000 */  'k',
    /*  200000 */  'l',
    /*  400000 */  'm',
    /*  800000 */  'n',
    /* 1000000 */  'o',
    /* 2000000 */  'p',
    /* 4000000 */  'q',
    /* 8000000 */  'r',
    /*10000000 */  'D',     // directory
    /*20000000 */  's',
    /*40000000 */  't',
    /*80000000 */  'u'
};


Index::Index(ntfs::Ntfs & ntfs)
: _ntfs(ntfs), _mftref(~0ULL)
{
}

Index::Index(ntfs::Ntfs & ntfs, u64 mftref)
: _ntfs(ntfs), _mftref(mftref)
{
    Init(_mftref);
}

void Index::Init(u64 mftref)
{

    std::vector<u8> recbuf(_ntfs.GetFileRecordSize());
    ntfs::FILE_RECORD_HEADER * precbuf = (ntfs::FILE_RECORD_HEADER*)&recbuf[0];

    _ntfs.ReadFileRecord(mftref, &recbuf[0]);
    if (precbuf->Ntfs.Type != ntfs::magic_FILE || !(precbuf->Flags & 1))
        throw std::runtime_error("MFT# is not a valid file or it is unused");

    Clear();

    ntfs::ATTRIBUTE * pattr     = (ntfs::ATTRIBUTE*)P_add(precbuf, precbuf->AttributesOffset);
    ntfs::ATTRIBUTE * pattrEnd  = (ntfs::ATTRIBUTE*)P_add(&recbuf[0], recbuf.size());
    for (; pattr < pattrEnd && pattr->AttributeType != eAttributeTerminator; pattr = P_add(pattr, pattr->Length))
    {
        switch (pattr->AttributeType)
        {
        case eAttributeIndexAllocation:
            AddFileList(pattr, pattrEnd);
            break;
        case eAttributeData:
            AddDataStream(pattr, pattrEnd);
            break;
        default:
            break;
        }
    }

}

//=============================================================================
// obtain the filelist from IndexAllocation block with name $I30
// no idea whether this block could appear twice.
void Index::AddFileList(ATTRIBUTE * pattr, ATTRIBUTE * /*pattrEnd*/)
{
    // this is the attr where all the files belongs to this dir is listed
    if (pattr->AttributeType != eAttributeIndexAllocation)
        throw std::runtime_error("Non index alloc block passed.");

    // check for attribute name L"$I30" - do nothing if it is not
    u16 * name = (u16*)P_add(pattr, pattr->NameOffset);
    if (pattr->NameLength != 4 || name[0] != L'$' || name[1] != L'I' || name[2] != L'3' || name[3] != L'0')
        return;

    // gets the attribute's data
    ntfs::AttributeData attr;
    attr.Init((u8*)pattr, pattr->Length);
    std::vector<u8> indexAlloc; // accumulated buffer for index_block data


    if (attr._nonResident)
    {
        // will be in external cluster (outside of MFT) if non-resident

        // the attr's data is datarun - parse the datarun
        ntfs::DataRun dr;
        dr.Init(&attr._data[0], attr._data.size(), attr._startVcn);

        // alloc buffers
        std::vector<u8> buf(_ntfs.GetBytesPerSector() * _ntfs.GetSectorsPerCluster());  // temporary reading buffer
        indexAlloc.reserve(buf.size());

        // reads and populate indexAlloc
        u64 startLcn;
        for (ntfs::DataRun::LIST::iterator it = dr._list.begin(); it != dr._list.end(); ++it)
        {
            startLcn = it->offset;
            for (u64 x = 0; x < it->count; ++x)
            {
                if (startLcn == 0)  // unallocated cluster of sparse file
                    memset(&buf[0], 0, buf.size());
                else
                    _ntfs.ReadLCN(startLcn + x, 1, &buf[0]);
                indexAlloc.insert(indexAlloc.end(), buf.begin(), buf.end());
            }
        }
    }
    else
    {
        // the data itself is the indexalloc data
        indexAlloc.assign(attr._data.begin(), attr._data.end());
    }

    // apply fixup
    if (!ApplyUpdateSequence(&indexAlloc[0], indexAlloc.size()))
        throw std::runtime_error("Can't apply fixup for ntfs Folder.");
    dump("index_alloc.bin", &indexAlloc[0], indexAlloc.size());

    // iterate through each file entry
    ntfs::INDEX_BLOCK_HEADER * pibh = (ntfs::INDEX_BLOCK_HEADER*)&indexAlloc[0];
    ntfs::DIRECTORY_INDEX * pindex = &pibh->DirectoryIndex;
    ntfs::DIRECTORY_ENTRY * pentry = (ntfs::DIRECTORY_ENTRY*)P_add(pindex, pindex->EntriesOffset);
    FILEHASH::iterator it;
    for (;pentry < (void*)P_add(&indexAlloc[0], indexAlloc.size()) && pentry->AttributeLength > 0; pentry = P_add(pentry, pentry->Length))
    {
        // skipped reserved files
        if ((pentry->FileReferenceNumber & MFT_MASK) < 16)
            continue;

        // adds file to filelist container
        it = _files.find(pentry->FileReferenceNumber);
        if (it == _files.end())
        {
            FolderElement fe;
            fe.attr = pentry->FName.FileAttributes;
            fe.mftref = pentry->FileReferenceNumber;
            if (pentry->FName.NameType & 0x2) fe.shortname.assign(pentry->FName.Name, pentry->FName.Name + pentry->FName.NameLength);
            if (pentry->FName.NameType & 0x1) fe.name.assign(pentry->FName.Name, pentry->FName.Name + pentry->FName.NameLength);
            _files.insert(std::make_pair(pentry->FileReferenceNumber, fe));
        }
        else
        {
            if (it->second.attr != pentry->FName.FileAttributes) throw std::runtime_error("Attribute not same.");
            if (it->second.mftref != pentry->FileReferenceNumber) throw std::runtime_error("MFT ref not same.");
            if (pentry->FName.NameType & 0x2) it->second.shortname.assign(pentry->FName.Name, pentry->FName.Name + pentry->FName.NameLength);
            if (pentry->FName.NameType & 0x1) it->second.name.assign(pentry->FName.Name, pentry->FName.Name + pentry->FName.NameLength);
        }

        //printf("\nf:%x", pentry->Flags);
        //printf(" L:%-3u(%x)", pentry->Length, pentry->Length);
        //printf(" aL:%-3u(%x)", pentry->AttributeLength, pentry->AttributeLength);
        //printf(" #:%-5I64u", pentry->FileReferenceNumber & MFT_MASK);
        //printf(" p:%-5I64u", pentry->FName.DirectoryFileReferenceNumber & MFT_MASK);
        //printf(" %c", (pentry->FName.FileAttributes & 0x10000000) ? 'D' : ' ');
        //printf(" [%x %.*S]", pentry->FName.NameType, pentry->FName.NameLength, pentry->FName.Name);
        //printf("\n");
    }
}

//=============================================================================
void Index::AddDataStream(ATTRIBUTE * pattr, ATTRIBUTE * /*pattrEnd*/)
{
    if (pattr->AttributeType != eAttributeData)
        throw std::runtime_error("Non data block passed.");

    ntfs::AttributeData attr;
    attr.Init((u8*)pattr, pattr->Length);

    STREAMHASH::iterator it;
    it = _streams.find(attr._attrName);
    if (it == _streams.end())
    {
        StreamElement se;
        if (attr._attrName.empty())
            se.name.push_back(L'?');
        else
            se.name = attr._attrName;
        se.nonresident = attr._nonResident;
        se.realsize = attr._nonResident ? attr._realSize : attr._attrLen;
        se.data.swap(attr._data);
        _streams.insert(std::make_pair(se.name, se));
    }
    else
    {
        // does it allow duplicate stream name?
        throw std::runtime_error("Duplicate stream name found.");
    }
}

//=============================================================================
bool Index::ApplyUpdateSequence(void * buf, u32 /*bufSize*/)
{
    const u32 BytesSkipped = 0x18;  // sizeof(NTFS_RECORD_HEADER) + sizeof(u64)
    u8 * bytes = (u8*)buf;
    if (bytes[0] == 'I' && bytes[1] == 'N' && bytes[2] == 'D' && bytes[3] == 'X')
    {
        ntfs::INDEX_BLOCK_HEADER * prec = (ntfs::INDEX_BLOCK_HEADER*)buf;
        u32 entryLen = prec->DirectoryIndex.IndexBlockLength + BytesSkipped;
        u32 allocatedLen = prec->DirectoryIndex.AllocatedSize + BytesSkipped;
        u32   usaOffset       = prec->Ntfs.UsaOffset;
        u32   usaCount        = prec->Ntfs.UsaCount - 1;
        u16 *  usaPos          = (u16*)buf + usaOffset/sizeof(u16);
        u16    usaChecksum     = *usaPos;
        u16 *  sectorLastWord  = (u16*)buf + (512/sizeof(u16) - 1);
        u32   usaSize = 0;

        if (allocatedLen < (usaCount * 512))
            return false;

        while (usaCount --> 0)
        {
            if (*sectorLastWord != usaChecksum)
            {
                // ignore bad update sequence if entry not affected by it.
                if (usaSize <= entryLen)
                    return false;
                break;
            }
            *sectorLastWord = *(++usaPos);

            sectorLastWord += 512/sizeof(u16);
            usaSize += 512;
        }
        return true;
    }
    return false;
}

//=============================================================================
void Index::Clear()
{
    _files.clear();
    _streams.clear();
    _mftref = ~0ULL;
}
