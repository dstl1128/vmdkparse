//
// VMDK Parser
// VMWare Disk parser with sector read capability.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//
// Currently supported disk type:
//   - monolithicSparse
//   - twoGbMaxExtentSparse
//   - monolithicFlat
//   - twoGbMaxExtentFlat
//
// Also supports opening snapshot-ed .vmdk files:
//   - will resolve through parent-link if needed
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <algorithm>

#include <assert.h>

#include "vmdk.h"
#include "ntfs.h"

using namespace disk;

char const * Vmdk::VMDK_TYPE_STR[] =
{
    "Unknown",
    "ZERO",
    "FLAT",
    "SPARSE",
    "VMFS",
    "VMFSSPARSE",
    "VMFSRDM",
    0,
};

Vmdk::VmdkType Vmdk::str2vmdktype(std::string const & s)
{
    char const ** p = Vmdk::VMDK_TYPE_STR;
    int i = 0;
    while (*p)
    {
        if (s.compare(*p) == 0)
            return (VmdkType)i;
        ++p;
        ++i;
    }
    return eUNKNOWN;
}

//=============================================================================
std::string strip(std::string const & s)
{
    std::string::size_type pos1 = s.find_first_not_of(' ');
    std::string::size_type pos2 = s.find_last_not_of(' ');
    return s.substr(pos1, pos2 - pos1 + 1);
}

//=============================================================================
Vmdk::Extent::Extent()
: sectors(0), offset(0), fp(IFile64::FileMaker())
{
}

void Vmdk::Extent::Clear()
{
    delete fp;
}

u32 Vmdk::Extent::GetGDE(u64 x)
{
    u64 index = x / (u64)seh.GetGtCoverage();
    u64 pos = (SECTOR_SIZE * (u64)seh.gdOffset) + (sizeof(u32) * index);
    u32 gde = 0;
    if (!fp->Seek(pos)) throw std::runtime_error("Seek error in GetGDE");
    if (sizeof(gde) != fp->Read(&gde, sizeof(gde))) throw std::runtime_error("GetGDE read error");
    return gde;
}

u32 Vmdk::Extent::GetGTE(u64 x, u32 gde)
{
    u64 index = (x % seh.GetGtCoverage()) / (u64)seh.grainSize;
    u64 pos = SECTOR_SIZE * (u64)gde + (sizeof(u32) * index);
    u32 gte = 0;
    if (!fp->Seek(pos)) throw std::runtime_error("Seek error in GetGTE");
    if (sizeof(gte) != fp->Read(&gte, sizeof(gte))) throw std::runtime_error("GetGTE read error");
    return gte;
}

bool Vmdk::Extent::RawSector(u64 x, void * buf)
{
    if (type == eSPARSE)
    {
        u32 gde = GetGDE(x);
        u32 gte = GetGTE(x, gde);
        if (gte > 0)
        {
            u64 index = x % (int)seh.grainSize;
            u64 pos = (SECTOR_SIZE * (u64)gte) + (SECTOR_SIZE * (u64)index);
            if (!fp->Seek(pos)) throw std::runtime_error("Can't seek while reading raw sector.");
            if (SECTOR_SIZE != fp->Read(buf, SECTOR_SIZE)) throw std::runtime_error("Can't read raw sector.");
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (type == eFLAT)
    {
        u64 pos = x * SECTOR_SIZE;
        if (!fp->Seek(pos)) throw std::runtime_error("Can't seek while reading raw sector.");
        if (SECTOR_SIZE != fp->Read(buf, SECTOR_SIZE)) throw std::runtime_error("Can't read raw sector.");
        return true;
    }
    throw std::runtime_error("Unsupported extent type while reading raw sector.");
    //return false; // unreachable code
}

//=============================================================================
Vmdk::Vmdk(std::string const & descriptorFilename)
: _descriptorFilename(descriptorFilename)
{
    Init();
}

Vmdk::~Vmdk()
{
    ExtentsArray::iterator it;
    for (it = _extents.begin(); it != _extents.end(); ++it)
        it->Clear();
}

void Vmdk::Test()
{
    //printf("Disk signature: %x\n", _mbr.diskSignature);
    //printf("Dummy: %x\n", _mbr.dummy);
    //printf("MBR signature: %x\n", _mbr.mbrSignature);
    //int part = 0;
    //for (Partitions::iterator it = _partitions.begin(); it != _partitions.end(); ++it, ++part)
    //{
    //    printf("%d. status=%x  type=%x  hd=%u  sec=%u  cyl=%u  1st=%llu  blocks=%llu\n",
    //        part,
    //        it->status,
    //        it->type,
    //        it->head,
    //        it->sector,
    //        it->cylinder,
    //        it->firstSectorLBA,
    //        it->numberBlock);
    //}
}

bool Vmdk::ReadSector(u64 x, void * buf, unsigned partitionNum)
{
    if (partitionNum >= _partitions.size())
        throw std::runtime_error("Partition number out of range.");
    x += _partitions[partitionNum].firstSectorLBA;
    return RawSector(x, buf);
}

bool Vmdk::ReadSectorN(u64 x, u32 count, void * buf, unsigned partitionNum)
{
    if (partitionNum >= _partitions.size())
        throw std::runtime_error("Partition number out of range.");
    u8* bytes = (u8*)buf;
    x += _partitions[partitionNum].firstSectorLBA; //_mbr.part[partitionNum].firstSectorLBA;
    while (count --> 0)
    {
        if (!RawSector(x, bytes))
            return false;
        ++x;
        bytes += SECTOR_SIZE;
    }
    return true;
}

bool Vmdk::RawSector(u64 sectorNumber, void * buf)
{
    // get the correct extents
    u64 x = sectorNumber;
    size_t i = 0;
    while (i < _extents.size())
    {
        if (x < _extents[i].sectors) // sector is within extents
            break;
        x -= _extents[i].sectors;
        ++i;
    }

    // and read the correct sector from it
    if (!_extents[i].RawSector(x, buf))
    {
        // if sector cannot be read
        //      either get from parent if available or
        //      zeroes the buffer
        if (_pParent.get())
        {
            if (!_pParent->RawSector(sectorNumber, buf))
                return false;
        }
        else
        {
            memset(buf, 0, SECTOR_SIZE);
        }
    }
    return true;
}

void Vmdk::Init()
{
    InitDescriptor();
    InitExtents();
    InitParent();
    InitPartition();
}

void Vmdk::InitPartition()
{
    // resets partition info
    _partitions.clear();

    // read Partition
    RawSector(0, &_mbr);
    if (_mbr.mbrSignature != 0xaa55)
        throw std::runtime_error("Invalid MBR signature.");

    // copy valid primary partition; ignore extended partition
    // extended partition = 0xf
    for (unsigned i = 0; i < ARR_LEN(_mbr.part); ++i)
    {
        if (_mbr.part[i].type != 0 && _mbr.part[i].type != 0xf)
            _partitions.push_back(_mbr.part[i]);
    }

    // process any logical partitions within extended partition
    for (unsigned i = 0; i < ARR_LEN(_mbr.part); ++i)
    {
        // extended partition type
        if (_mbr.part[i].type == 0xf)
            InitExtendedPartition(_mbr.part[i].firstSectorLBA, _mbr.part[i].numberBlock);
    }
}

void Vmdk::InitExtendedPartition(u64 ebrSector, u64 ebrLeft)
{
    disk::Ebr ebr;
    if (!RawSector(ebrSector, &ebr))
        throw std::runtime_error("Can't read extended boot record.");
    if (ebr.mbrSignature != 0xaa55)
        throw std::runtime_error("Invalid EBR signature.");

    disk::Partition part1(ebr.part[0]);
    disk::Partition part2(ebr.part[1]);

    // get 1st valid partition
    part1.firstSectorLBA += ebrSector;
    _partitions.push_back(part1);

    // process next EBR chain if available
    if (part2.firstSectorLBA != 0 && part2.numberBlock != 0)
    {
        u64 nextEbrSector = ebrSector + part2.firstSectorLBA;
        u64 nextEbrLeft = ebrLeft - part2.firstSectorLBA;
        InitExtendedPartition(nextEbrSector, nextEbrLeft);
    }
}

void Vmdk::InitParent()
{
    static const std::string s_parentFileNameHint("parentFileNameHint");
    Properties::iterator it = _properties.find(s_parentFileNameHint);
    if (it != _properties.end())
    {
        std::string fullPath(_basePath);
        fullPath.append(it->second);
        _pParent.reset(new Vmdk(fullPath));
    }
}

void Vmdk::InitExtents()
{
    // get base path
    std::string::size_type pos = _descriptorFilename.find_last_of(SEPS);
    if (pos != std::string::npos)
        _basePath = _descriptorFilename.substr(0, pos+1);

    // go thru every extents
    ExtentsArray::iterator it;
    std::string fullPath;
    for (it = _extents.begin(); it != _extents.end(); ++it)
    {
        fullPath.assign(_basePath);
        fullPath.append(it->filename);

        //std::cout << "-------------------------------------------\n";
        //std::cout << "Opening extents: " << fullPath << std::endl;
        it->fp->Open(fullPath.c_str());
        if (!it->fp->IsOpen())
            throw std::runtime_error("Can't open extents VMDK");

        // only sparse file type have sparse extent header (SEH)
        if (it->type == eSPARSE)
        {
            ReadSeh(it->seh, *it->fp);
            if (it->seh.capacity != it->sectors)
                throw std::runtime_error("Capacity not as advertised.");
        }
    }
}

void Vmdk::ReadSeh(SparseExtentHeader & seh, IFile64 & ifs)
{
    ifs.Read(&seh, sizeof(seh));
    if (seh.magicNumber != 0x564d444b)
        throw std::runtime_error("Corrupted VMDK file given or format not suppported.");

    //printf("Sizeof seh: %lu\n", sizeof(seh));
    //printf("Magic number: %x\n", seh.magicNumber);
    //printf("Version: %x\n", seh.version);
    //printf("Flags: %x\n", seh.flags);
    //printf("Capacity: %llu\n", seh.capacity);
    //printf("Grain size: %llu\n", seh.grainSize);
    //printf("***** DESCRIPTOR OFFSET: %llu\n", seh.descriptorOffset);
    //printf("***** DESCRIPTOR SIZE: %llu\n", seh.descriptorSize);
    //printf("No. of GTE per GT: %u\n", seh.numGTEsPerGT);
    //printf("Rgd offset: %llu\n", seh.rgdOffset);
    //printf("Gd offset: %llu\n", seh.gdOffset);
    //printf("Overhead: %llu\n", seh.overHead);
    //printf("Unclean shutdown: %d\n", seh.uncleanShutdown);
    //printf("Single endline char: %d\n", (int)seh.singleEndLineChar);
    //printf("Non endline char: %d\n", (int)seh.nonEndLineChar);
    //printf("Double endline char1: %d\n", (int)seh.doubleEndLineChar1);
    //printf("Double endline char2: %d\n", (int)seh.doubleEndLineChar2);
    //printf("Compression Algo: %hu\n", seh.compressAlgorithm);
    //printf("Gt coverage: %llu\n", seh.GetGtCoverage());
}


void Vmdk::InitDescriptor()
{
    std::vector<char> buf;

    // opens descriptor file
    {
        std::auto_ptr<IFile64> fp(IFile64::FileMaker());
        fp->Open(_descriptorFilename.c_str());
        if (!fp->IsOpen())
            throw std::runtime_error("Unable to open descriptor file.");

        char hdr[4];
        if (fp->Read(hdr, 4) != 4)
            throw std::runtime_error("Can't read empty file.");

        fp->Seek(0);
        if (strncmp(hdr, "KDMV", 4) == 0)
        {
            // descriptor is embedded
            SparseExtentHeader seh;
            ReadSeh(seh, *fp);
            if (seh.descriptorOffset > 0)
            {
                u64 pos = seh.descriptorOffset * SECTOR_SIZE;
                size_t size = (size_t)(seh.descriptorSize * SECTOR_SIZE);
                buf.resize(size);
                if (!fp->Seek(pos)) throw std::runtime_error("Seeking error");
                if (size != fp->Read(&buf[0], size))
                    throw std::runtime_error("Sudden Eof for embedded descriptor file");
            }
            else
                throw std::runtime_error("No descriptor offset in SEH.");
        }
        else
        {
            // descriptor is external
            // limits to 1MB of external descriptor file
            if (fp->Size() > 1048576)
                throw std::runtime_error("Can't handle VMDK having very large descriptor file.");

            // since it is <= 1MB, cast is safe
            size_t size = (size_t)fp->Size();
            buf.resize(size);
            if (size != fp->Read(&buf[0], size))
                throw std::runtime_error("Sudden Eof for individual descriptor file");
        }
    }

    // parses the descriptor
    std::string s(buf.begin(), buf.end());
    std::istringstream iss(s);
    ParseDescriptor(iss);
}


void Vmdk::ParseDescriptor(std::istream & ifs)
{
    std::string s;
    int state = 0;  // 1=descriptor, 2=extens, 3=disk data, 4=ddb
    while (!ifs.eof())
    {
        std::getline(ifs, s);
        if (s.empty())
            continue;

        if (s[0] == '#')
        {
            if (state == 3 && s.find("#DDB") == 0)                  state = 4;
            if (state == 2 && s.find("# The Disk Data Base") == 0)  state = 3;
            if (state == 1 && s.find("# Extent description") == 0)  state = 2;
            if (state == 0 && s.find("# Disk DescriptorFile") == 0) state = 1;
            continue;
        }

        switch (state)
        {
        case 1: case 3: case 4:
            // reads descriptor as properties
            {
                std::string::size_type pos = s.find_first_of('=');
                if (pos != -1)
                {
                    std::string key = strip(s.substr(0, pos));
                    std::string val = strip(s.substr(pos+1));
                    if (val[0] == '"' && val[val.length()-1] == '"')
                        val = val.substr(1, val.length()-2);
                    //std::cout << key << ":" << val << std::endl;

                    // TODO: check for redefinition of key:
                    // Properties::iterator it = _properties.find(key);
                    //
                    _properties[key] = val;
                }
            }
            break;

        case 2:
            // read extends
            {
                std::istringstream iss(s);
                Extent ext;
                std::string stype;
                iss >> ext.access >> ext.sectors >> stype;
                ext.type = str2vmdktype(stype);
                std::getline(iss, ext.filename, '"');   // read until the first "
                std::getline(iss, ext.filename, '"');   // read till the next "
                iss >> ext.offset;
                //std::cout << ext.access << ' ' << ext.sectors << ' ';
                //std::cout << ext.type << ' ' << ext.filename << ' ';
                //std::cout << ext.offset << std::endl;
                _extents.push_back(ext);
            }
            break;

        default:
            //std::cout << "[" << s << "]" << std::endl;
            break;
        }
    } // while (!ifs.eof())
}
