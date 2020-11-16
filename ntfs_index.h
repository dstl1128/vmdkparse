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

#ifndef __NTFS_INDEX_H
#define __NTFS_INDEX_H

#include "ntfs.h"

#include <string>
#include <map>
#include <vector>

namespace ntfs
{
    u32 const eFileAttributeDirectory = 0x10000000;

    struct FolderElement
    {
        u64 mftref;
        u32 attr;   // this is a rough file attribute - might not be up to date
        std::basic_string<u16> shortname;
        std::basic_string<u16> name;
    };

    struct StreamElement
    {
        std::basic_string<u16> name;
        std::vector<u8> data;   // either datarun (non-resident) or the actual data (resident)
        u64 realsize;
        u8 nonresident;
    };

    class Index
    {
    public:
        typedef std::map<u64, FolderElement> FILEHASH;      // hash by mft ref number
        typedef std::map<std::basic_string<u16>, StreamElement> STREAMHASH;     // hash by data stream name

        Index(ntfs::Ntfs & ntfs);
        Index(ntfs::Ntfs & ntfs, u64 mftref);
        void Init(u64 mftref);
        void Clear();

        // files list - only available when entry is a folder
        // otherwise it is empty... unless something weird happens?
        u32 FilesCount() const { return _files.size(); }
        FILEHASH::iterator FilesBegin() { return _files.begin(); }
        FILEHASH::iterator FilesEnd() { return _files.end(); }
        FILEHASH & Files() { return _files; }

        // data streams - only available when entry is a file
        // must have at least a nameless data stream
        u32 StreamsCount() const { return _streams.size(); }
        STREAMHASH::iterator StreamsBegin() { return _streams.begin(); }
        STREAMHASH::iterator StreamsEnd() { return _streams.end(); }
        STREAMHASH & Streams() { return _streams; }

    private:
        //Index & operator = (Index const &) { return *this; }    // assignment not allowed

        void AddFileList(ATTRIBUTE * pattr, ATTRIBUTE * pattrEnd);
        void AddDataStream(ATTRIBUTE * pattr, ATTRIBUTE * pattrEnd);
        bool ApplyUpdateSequence(void * buf, u32 bufSize);

        ntfs::Ntfs & _ntfs;
        u64 _mftref;

        FILEHASH _files;
        STREAMHASH _streams;
    };
}

#endif // __NTFS_INDEX_H
