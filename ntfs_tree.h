//
// NTFS Tree
// Parses the entire $MFT file and
// reconstructs files and folders hierarchy.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __NTFS_TREE_H
#define __NTFS_TREE_H

#include "ntfs.h"

#include <map>
#include <deque>
#include <string>
#include <set>

namespace ntfs
{
    // basic element of a data stream
    // contains name, size and data
    struct Stream
    {
        std::basic_string<u16> name;
        std::vector<u8> data;   // either datarun (non-resident) or the actual data (resident)
        ntfs::DataRun dataRun;
        u16 attrId;
        u64 realSize;
        u8 nonResident;

        bool compressed;
        u16 compressUnitSize;   // ??
        u64 compressSize;   // size of compressed data

        Stream() : attrId(0), realSize(0), nonResident(0), compressed(false), compressUnitSize(0), compressSize(0) { }
        void Clear() { name.clear(); data.clear(); dataRun.Clear(); realSize = compressSize = attrId = compressUnitSize = nonResident = 0; compressed = false;}
        bool IsEmpty() const { return !realSize; }
    };

    // stream container
    typedef std::map<std::basic_string<u16>, Stream> STREAMS;

    // basic element of a file or folder that
    // contains basic information and
    // a list of data streams if available
    struct Node
    {
        u64 mftRef;
        u64 parentRef; // parent MFT ref index
        u32 attr; // file attributes
        int isdir;  // non-zero if is dir
        std::basic_string<u16> shortname;
        std::basic_string<u16> name;
        STREAMS streams;
        Node() : mftRef(0), parentRef(0), attr(0) { }
        void Clear() { shortname.clear(); name.clear(); streams.clear(); mftRef = parentRef = attr = 0; }
        bool IsEmpty() const { return !mftRef || !parentRef || !attr; }
    };

    // a file container -> folder
    typedef std::deque<Node> NODES;

    // a folder container indexed by folder's MFT reference
    typedef std::map<u64, NODES> FOLDERS;

    // a parent map for each MFT record
    typedef std::map<u64, u64> PARENTMAP;


    // forward declare
    class File;

    class Tree
    {
        friend class File;  // only friends can touch private parts
    public:
        Tree(ntfs::Ntfs & ntfs);
        void Print(wchar_t const * prefixDir, std::ostream & os = std::cout, u64 folderMftIndex = 5);
        void Print(char const * prefixDir, std::ostream & os = std::cout, u64 folderMftIndex = 5);

    private:
        void Init();
        void PrintInternal(std::string const & prefixDir, std::ostream & os, u64 folderMftIndex);
        void ProcessAttribute(ATTRIBUTE * pattr, ATTRIBUTE * pattrEnd, Node & node, u64 listref = 0, u16 attrNum = 0);
        //ntfs::Tree & operator = (ntfs::Tree &) { return *this; }    // not allow assignment

        ntfs::Ntfs & _ntfs;
        FOLDERS _folders;
        PARENTMAP _parentMap;
    };
}


#endif // __NTFS_TREE_H
