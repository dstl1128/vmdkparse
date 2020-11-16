//
// NTFS Attribute
// Obtain the attribute's raw content by either reading it from
// within MFT if resident or, reading it outside of current
// MFT record if it is non-resident.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __NTFS_ATTR_H
#define __NTFS_ATTR_H

#include "types.h"
#include "ntfs_layout.h"

#include <iostream>
#include <string>
#include <vector>

namespace ntfs
{
    class Attribute
    {
    public:
        Attribute();
        void Clear();
        void Init(u8 * buf, u32 size);
        void Print();
        u64 GetDataLength() const;

        // standard attr
        ATTRIBUTE_TYPE _attrType;
        u32  _length;
        u8   _nonResident;
        u8   _nameLen;
        u16  _flags;
        u16  _attrId;
        std::basic_string<u16> _attrName;
        //std::string _utf8AttrName;

        // resident attr
        u32  _attrLen;
        u16  _attrOffset;

        // non-resident attr
        u64  _startVcn;
        u64  _endVcn;
        u16  _dataRunOffset;
        u16  _compressionUnitSize;
        u64  _allocatedSize;
        u64  _realSize;
        u64  _compressSize;
    };

    //=============================================================================
    class AttributeData : public Attribute
    {
    public:
        AttributeData();
        void Clear();
        void Init(u8 * buf, u32 size);

        std::vector<u8> _data;
    };

    //=============================================================================
    class AttributeList : public Attribute
    {
    public:
        AttributeList();
        void Clear();
        void Init(u8 * buf, u32 size);

        std::vector<u8> _data;
    };
}

#endif // __NTFS_ATTR_H
