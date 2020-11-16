//
// Disk Reader Interface
// Provides common disk read interface only.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __IDISKREAD_H
#define __IDISKREAD_H

#include "types.h"

#include <deque>

namespace disk
{

#pragma pack(push, 1)
    struct MbrPartition
    {
        u8       status;             // 0x80=bootable; 0x00=non-bootable; other=invalid
        // CHS of first block in partition
        u8       head;
        u8       sector;             // sector in 5-0 bits; 9-8 of cylinder is in 7-6 bits here
        u8       cylinder;           // 7-0 bits of cylinder
        u8       type;
        // CHS of last block in partition
        u8       headLast;
        u8       sectorLast;
        u8       cylinderLast;
        u32      firstSectorLBA;     // LBA of first sector of partition
        u32      numberBlock;        // number of blocks in partition
    };

    // Master Boot Record
    struct Mbr
    {
        u8      code[440];          // code area
        u32     diskSignature;      // optional
        u16     dummy;              // usually null = 0x0000
        MbrPartition   part[4];     // partition info
        u16     mbrSignature;       // 0xAA55
    };

    // Extended Boot Record
    struct Ebr
    {
        u8      code[446];          // generally unused
        MbrPartition    part[2];    // first & second entry
        u8      dummy2[32];          // unused
        u16     mbrSignature;       // 0xAA55
    };


#pragma pack(pop)

    struct Partition
    {
        u32     type;
        u32     status;
        u32     head;
        u32     sector;
        u32     cylinder;
        u64     firstSectorLBA;
        u64     numberBlock;
        Partition();
        Partition(MbrPartition const & mbrpart);
    };
    typedef std::deque<Partition> Partitions;


    class IDiskRead
    {
    public:
        virtual bool RawSector(u64 x, void * buf) = 0;
        virtual bool ReadSector(u64 x, void * buf, unsigned partitionNum=0) = 0;
        virtual bool ReadSectorN(u64 x, u32 count, void * buf, unsigned partitionNum=0) = 0;
    };

}


#endif // __IDISKREAD_H
