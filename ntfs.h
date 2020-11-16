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

#ifndef __NTFS_H
#define __NTFS_H

#include "idiskread.h"
#include "ntfs_attr.h"
#include "ntfs_datarun.h"
#include "ntfs_layout.h"

#include <memory>

#define INVALID_CLUSTER_VALUE ~0
#define MFT_MASK 0x0000ffffffffffffULL
#define MFT_MASK_BITS 48

namespace ntfs
{


//=============================================================================
    class Tree;

    class Ntfs
    {
        friend class Tree;
    public:
        Ntfs(disk::IDiskRead & disk, int partitionNum=0);

        bool ApplyUpdateSequence(void * buf, u32 bufSize);
        void ReadFileRecord(u64 index, void * phdr);
        void ReadLCN(u64 lcn, u32 count, void * buf);
        u32 GetFileRecordSize() const { return _bytesPerFileRecord; }
        u64 GetMftSize() const { return _mftSize; }
        u64 GetMftAllocatedSize() const { return _mftAllocatedSize; }
        u64 GetMftStartVcn() const { return _mftStartVcn; }
        u64 GetMftEndVcn() const { return _mftEndVcn; }
        u16 GetBytesPerSector() const { return _bootb.BytesPerSector; }
        u8 GetSectorsPerCluster() const { return _bootb.SectorsPerCluster; }
        void Test();

    private:
        void InitBoot();
        void InitMft();
        void Init();
        ATTRIBUTE * FindAttribute(FILE_RECORD_HEADER * phdr, ATTRIBUTE_TYPE type, u16 const * name);
        //ntfs::Ntfs & operator = (ntfs::Ntfs const &) { return *this; }  // not allow assignment

        disk::IDiskRead & _disk;
        int _partitionNum;
        BOOT_BLOCK _bootb;
        std::auto_ptr<AttributeData> _pAttrData;
        std::auto_ptr<AttributeList> _pAttrList;
        std::auto_ptr<DataRun> _pMftDataRun;
        std::vector<u8> _mft;
        u32 _bytesPerFileRecord;
        u64 _mftSize;
        u64 _mftAllocatedSize;
        u64 _mftStartVcn;
        u64 _mftEndVcn;
    };

}


#endif // __NTFS_H
