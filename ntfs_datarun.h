//
// NTFS Data Run
// Data Runs describe where is the file's content being recorded on which
// clusters. This module parses the Data Run and provide conversion of
// Virtual Cluster Number (VCN) to Logical Cluster Number (LCN).
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __NTFS_DATARUN_H
#define __NTFS_DATARUN_H

#include "idiskread.h"

#include <vector>


namespace ntfs
{
    u32 RunLength(u8 * run);
    s64 RunLCN(u8 * run);
    u64 RunCount(u8 * run);

    struct DataRunElement
    {
        u64 count;      // number of cluster in the cluster group
        u64 offset;     // cluster offset of the cluster group
        u64 cumulativeOffset;   // calculated cumulative offset

        DataRunElement() : count(0), offset(0), cumulativeOffset(0) { }
    };

    class DataRun
    {
    public:
        typedef std::vector<DataRunElement> LIST;

        DataRun();
        void Clear();
        bool Empty() const;
        void Init(u8 * buf, u32 size, u64 baseVcn);
        void Append(u8 * buf, u32 size, u64 startVcn);
        u64 Vcn2Lcn(u64 x);

    public:
        u64  _baseVcn;
        LIST _list;
    };

}

#endif // __NTFS_DATARUN_H
