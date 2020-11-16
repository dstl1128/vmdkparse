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

#include "ntfs_datarun.h"

#include <stdexcept>

using namespace ntfs;


/**
 * DataRun format:
 *
 * [1-byte indicator][n-bytes cluster_count][m-bytes cluster_offset]
 *
 * DataRun ends when indicator byte = 0x00
 *
 * m = indicator & 0xf = the sizeof(DataRun's cluster_count)
 * n = (indicator >> 4) & 0xf = the sizeof(DataRun's relative cluster_offset)
 *
 * cluster_offset:
 *   - can be negative very badly fragmented files
 *   - can be zero if sparse file (n=0, no physical cluster allocated)
 *   - always relative to the previous runs
 *
 *
 * Types of DataRun:
 *   - Normal, unfragmented files
 *   - Normal, fragmented files
 *   - Normal, scrambled files (badly fragmented)
 *   - Sparse, unfragmented files
 *   - Sparse, fragmented files
 *   - Compressed, sparse, fragmented files (not supported)
 */

u32 ntfs::RunLength(u8 * run)
{
    // (sizeof cluster_count) + (sizeof cluster_offset) + (indicator byte == *run itself)
    return (*run & 0xf) + ((*run >> 4) & 0xf) + 1;
}


s64 ntfs::RunLCN(u8 * run)
{
    // reads byte-by-byte until it fills up the cluster_offset
    u8 len = *run & 0xf;
    u8 off = (*run >> 4) & 0xf;
    s64 lcn = (off == 0) ? 0 : (s8)run[len + off];
    for (s32 i = len + off - 1; i > len; i--)
        lcn = (lcn << 8) + run[i];
    return lcn;
}

u64 ntfs::RunCount(u8 * run)
{
    // reads byte-by-bytes until it fills up the length
    u8 len = *run & 0xf;
    u64 count = 0;
    for (u32 i = len; i > 0; i--)
        count = (count << 8) + run[i];
    return count;
}


//=============================================================================
DataRun::DataRun() : _baseVcn(0)
{
    _list.reserve(8);     // just reserving some amount of elements
}

bool DataRun::Empty() const
{
    return _list.empty();
}

void DataRun::Init(u8 * buf, u32 size, u64 baseVcn)
{
    u8 * bufend = buf + size;
    u64 runCount;
    s64 runOffset;
    u64 cumulativeOffset = 0;
    _baseVcn = baseVcn;

    // starts the decoding
    DataRunElement dr;
    while (buf < bufend && *buf)
    {
        runCount = RunCount(buf);
        runOffset = RunLCN(buf);

        cumulativeOffset += runOffset;

        dr.count = runCount;
        dr.offset = runOffset;
        dr.cumulativeOffset = cumulativeOffset;
        _list.push_back(dr);

        buf += RunLength(buf);
    }
}

void DataRun::Append(u8 * buf, u32 size, u64 startVcn)
{
    if (_list.empty())
        throw std::runtime_error("Must initialize DataRun first before append.");

    u8 * bufend = buf + size;
    u32 i;
    u64 runCount;
    s64 runOffset;
    u64 cumulativeOffset = _baseVcn;

    // check if given datarun extends the previous one
    for (i = 0; i < _list.size(); ++i)
        cumulativeOffset += _list[i].count;
    if (cumulativeOffset != startVcn)
        throw std::runtime_error("Given buffer does not extend this DataRun.");
    cumulativeOffset = 0;

    // starts the decoding
    DataRunElement dr;
    while (buf < bufend && *buf)
    {
        runCount = RunCount(buf);
        runOffset = RunLCN(buf);

        cumulativeOffset += runOffset;
        dr.count = runCount;
        dr.offset = runOffset;
        dr.cumulativeOffset = cumulativeOffset;
        _list.push_back(dr);

        buf += RunLength(buf);
    }
}

// returns 0 if Vcn lands on a sparse cluster
u64 DataRun::Vcn2Lcn(u64 x)
{
    if (_list.empty())
        throw std::runtime_error("Vcn2Lcn can't operate on empty DataRun.");

    u32 i = 0;
    u64 vcn = x + _baseVcn;

    // follow list until hit the request vcn
    while (i < _list.size())
    {
        if (vcn < _list[i].count)   // vcn is within the cluster count of current runs
            break;
        vcn -= _list[i].count;
        ++i;
    }

    if (i == _list.size())
        throw std::runtime_error("Vcn2Lcn can't find correct cluster number.");

    return (_list[i].offset == 0) ? 0 : (vcn + _list[i].cumulativeOffset);
}

void DataRun::Clear()
{
    _baseVcn = 0;
    _list.clear();
}
