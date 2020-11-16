//
// NTFS File
// Base on the result of ntfs::Tree, provides generic 64bit file
// reading interfaces with Win32-like path target, and reads
// the file content from the NTFS file system.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __NTFS_FILE_H
#define __NTFS_FILE_H

#include "types.h"
#include "ntfs.h"
#include "ntfs_tree.h"
#include "file64.h"

#include <vector>

namespace ntfs
{
    class File : public IFile64
    {
    public:
        File(ntfs::Tree & tree);

        bool Open(char const * filename);
        bool Open(wchar_t const * filename);    // filename have to start with root, e.g. L"\Windows\System32\kernel32.dll"
        void Close();
        bool IsOpen() const;
        bool Eof() const;
        unsigned long Read(void * buf, unsigned long size);
        bool Seek(s64 pos, u32 moveMethod = 0);
        s64 Size() const;

    private:
        //ntfs::File & operator = (ntfs::File const &) { return *this; } // not allowed
        void Validate() const;
        bool OpenInternal(std::basic_string<u16> const & filename);

        ntfs::Tree & _tree; // can touch Tree private parts because we are friends
        ntfs::Ntfs & _ntfs;
        ntfs::Node _node;
        ntfs::Stream _stream;
        u64 _pos;

        // for compression used
        u32 _clustersPerGroup;
        std::vector<u8> _compressBuf;

        // for caching
        u64 _oldClusterNumber;
        std::vector<u8> _clusterBuf;
    };
}

#endif  // __NTFS_FILE_H
