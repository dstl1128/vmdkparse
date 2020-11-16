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

#ifndef __VMDK_H
#define __VMDK_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <memory>

#include "types.h"
#include "file64.h"
#include "idiskread.h"

#define SECTOR_SIZE 512

namespace disk
{
    typedef u64 SectorType;
    typedef u8 Bool;

    unsigned int const CID_NOPARENT = ~(0x0U);

    class Vmdk : public IDiskRead
    {
        typedef std::map<std::string, std::string> Properties;

        //struct Descriptor
        //{
        //    unsigned int    version;    // version 1
        //    std::string     encoding;   // for now = "windows-1252"
        //    unsigned int    cid;        // content ID
        //    unsigned int    parentcid;  // CID_NOPARENT if no parent
        //    std::string     createtype; // monolithicFlat, monolithicSparse, twoGbMaxExtentFlat, twoGbMaxExtentSparse
        //};


        //struct DiskDataBase
        //{
        //    std::string     adapter;    // ide, buslogic, lsilogic
        //    std::string     sectors;    // geometry sectors
        //    std::string     heads;      // geometry heads
        //    std::string     cylinders;  // geometry cylinder
        //};

#pragma pack(push, 1)
        struct SparseExtentHeader
        {
            u32     magicNumber;
            u32     version;
            u32     flags;
            u64     capacity;
            u64     grainSize;
            u64     descriptorOffset;
            u64     descriptorSize;
            u32     numGTEsPerGT;
            u64     rgdOffset;
            u64     gdOffset;
            u64     overHead;
            u8      uncleanShutdown;
            char    singleEndLineChar;
            char    nonEndLineChar;
            char    doubleEndLineChar1;
            char    doubleEndLineChar2;
            u16     compressAlgorithm;
            u8      pad[433];
            u64 GetGtCoverage() const { return numGTEsPerGT * grainSize; }
        };
#pragma pack(pop)

        enum VmdkType
        {
            eUNKNOWN,
            eZERO,
            eFLAT,
            eSPARSE,
            eVMFS,
            eVMFSSPARSE,
            eVMFSRDM,
            eVmdkTypeCount,
        };
        static char const * VMDK_TYPE_STR[];
        struct Extent
        {
            std::string     access;     // RW, RDONLY, NOACCESS
            u64             sectors;    // number of sectors
            VmdkType        type;       // FLAT or SPARSE (ZERO, VMFS, VMFSSPARSE, VMFSRDM not supported)
            std::string     filename;   // extent's filename
            u64             offset;     // for FLAT extents
            SparseExtentHeader seh;     // sparse extent header


            IFile64 * fp;   // TODO: this must be exception safe
            Extent();
            void Clear();
            u32 GetGDE(u64 x);
            u32 GetGTE(u64 x, u32 gde);
            bool RawSector(u64 x, void * buf);
        };
        typedef std::deque<Extent> ExtentsArray;


    public:
        static VmdkType str2vmdktype(std::string const & s);

        Vmdk(std::string const & descriptorFile);
        ~Vmdk();

        virtual bool RawSector(u64 x, void * buf);
        virtual bool ReadSector(u64 x, void * buf, unsigned partitionNum=0);
        virtual bool ReadSectorN(u64 x, u32 count, void * buf, unsigned partitionNum = 0);
        void Test();
        disk::Partitions::iterator BeginPartition() { return _partitions.begin(); }
        disk::Partitions::iterator EndPartition() { return _partitions.end(); }

    private:
        void Init();
        void InitDescriptor();
        void InitExtents();
        void InitParent();
        void InitPartition();
        void InitExtendedPartition(u64 ebrSector, u64 ebrLeft);
        void ReadSeh(SparseExtentHeader & seh, IFile64 & ifs);
        void ParseDescriptor(std::istream & ifs);

    private:
        std::string _descriptorFilename;
        std::string _basePath;
        ExtentsArray _extents;
        Properties _properties;
        SparseExtentHeader _seh;
        std::auto_ptr<Vmdk> _pParent;
        Mbr _mbr;
        disk::Partitions _partitions;
    };

}

#endif // __VMDK_H
