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

#include "ntfs_attr.h"

#include "utf8.h"

#include <stdexcept>
#include <vector>
#include <string>

using namespace ntfs;

Attribute::Attribute()
{
    Clear();
}

void Attribute::Init(u8 * buf, u32 size)
{
    if (size < 0x10)
        throw std::runtime_error("All std header have at least 16byte common info");

    ATTRIBUTE * pattr = (ATTRIBUTE*)buf;

    _attrType = pattr->AttributeType;
    _length = pattr->Length;
    _nonResident = pattr->Nonresident;
    _nameLen = pattr->NameLength;
    _flags = pattr->Flags;
    _attrId = pattr->AttributeNumber;

    if (_nonResident)
    {
        if (size < 0x40)
            throw std::runtime_error("Not enough data for non-resident attribute.");
    }
    else
    {
        if (size < 0x18)
            throw std::runtime_error("Not enough data for resident attribute.");
    }

    if (_nameLen > 0)
    {
        if ( (u32)(pattr->NameOffset + _nameLen*sizeof(u16)) > size)
            throw std::runtime_error("Attribute name is too large.");

        std::vector<u16> tempname(_nameLen);
        memcpy(&tempname[0], buf + pattr->NameOffset, _nameLen * sizeof(u16));

        _attrName.clear();
        _attrName.assign(tempname.begin(), tempname.end());

        //_utf8AttrName.clear();
        //utf8::utf16to8(tempname.begin(), tempname.end(), std::back_inserter(_utf8AttrName));

        //printf("Attribute name: [%S]\n", _attrName.c_str());
    }

    if (_nonResident)
    {
        NONRESIDENT_ATTRIBUTE * pNRAttr = (NONRESIDENT_ATTRIBUTE*)pattr;
        _startVcn = pNRAttr->StartVcn;
        _endVcn = pNRAttr->LastVcn;
        _dataRunOffset = pNRAttr->DataRunOffset;
        _compressionUnitSize = pNRAttr->CompressionUnitSize;
        _allocatedSize = pNRAttr->AllocatedSize;
        _realSize = pNRAttr->RealSize;

        // if compressed, there's one more u64 at the end
        if (_flags & 0x1)
        {
            // it is at the end of NONRESIDENT_ATTRIBUTE
            memcpy(&_compressSize, P_add(pNRAttr, sizeof(NONRESIDENT_ATTRIBUTE)), sizeof(_compressSize));
        }
    }
    else
    {
        RESIDENT_ATTRIBUTE * pRAttr = (RESIDENT_ATTRIBUTE*)pattr;
        _attrLen = pRAttr->ValueLength;
        _attrOffset = pRAttr->ValueOffset;
    }
}

void Attribute::Clear()
{
    _attrType = eAttributeTerminator;
    _length = 0;
    _nonResident = 0;
    _nameLen = 0;
    _flags = 0;
    _attrId = 0;
    _attrName.clear();

    _attrLen = 0;
    _attrOffset = 0;

    _startVcn = 0;
    _endVcn = 0;
    _dataRunOffset = 0;
    _allocatedSize = 0;
    _realSize = 0;
    _compressionUnitSize = 0;
    _compressSize = 0;
}

u64 Attribute::GetDataLength() const
{
    return _nonResident ? _realSize : _attrLen;
}

void Attribute::Print()
{
    printf("\nAttribute type: %#x %s\n", _attrType, ntfs::attrtype2str(_attrType));
    printf("Attribute length: %u\n", _length);
    printf("Resident?: %s\n", _nonResident?"No":"Yes");
    printf("Name length: %u\n", _nameLen);
    printf("Attribute name: %S\n", _attrName.empty() ? L"[unnamed]": (wchar_t*)&_attrName[0]);
    printf("Flags: %x\n", _flags);
    printf("Id: %x\n", _attrId);

    if (_nonResident)
    {
        printf("Start vcn: %llu\n", _startVcn);
        printf("End vcn: %llu\n", _endVcn);
        printf("Data run offset: %u\n", _dataRunOffset);
        printf("Compression Unit size: %u\n", _compressionUnitSize);
        printf("Allocated size: %llu\n", _allocatedSize);
        printf("Real size: %llu\n", _realSize);
    }
    else
    {
        printf("Attribute data length: %u\n", _attrLen);
        printf("Attribute offset: %u\n", _attrOffset);
    }
}

//=============================================================================
// extract out the data of the attribute
AttributeData::AttributeData()
{
    Clear();
}


void AttributeData::Init(u8 * buf, u32 size)
{
    Attribute::Init(buf, size);

    if (_nonResident)
    {
        u32 dataSize = _length - _dataRunOffset;
        if (dataSize > 0)
        {
            if (_dataRunOffset + dataSize > size)
                throw std::runtime_error("Not enough buffer for non-resident data attr.");
            _data.resize(dataSize);
            memcpy(&_data[0], buf + _dataRunOffset, dataSize);
        }
    }
    else
    {
        u32 dataSize = _attrLen;
        if (dataSize > 0)
        {
            if (_attrOffset + dataSize > size)
                throw std::runtime_error("Not enough buffer for resident data attr.");
            _data.resize(dataSize);
            memcpy(&_data[0], buf + _attrOffset, dataSize);
        }
    }
}

void AttributeData::Clear()
{
    Attribute::Clear();
    _data.clear();
}

//=============================================================================
// $ATTRIBUTE_LIST
AttributeList::AttributeList()
{
    Clear();
}

void AttributeList::Init(u8 * buf, u32 size)
{
    Attribute::Init(buf, size);

    if (_nonResident)
    {
        u32 dataSize = _length - sizeof(NONRESIDENT_ATTRIBUTE) - _nameLen*sizeof(u16);
        if (dataSize > 0)
        {
            if (_dataRunOffset + dataSize > size)
                throw std::runtime_error("Not enough buffer for non-resident list attr.");
            _data.resize(dataSize);
            memcpy(&_data[0], buf + _dataRunOffset, dataSize);
        }
    }
    else
    {
        u32 dataSize = _attrLen;
        if (dataSize > 0)
        {
            if (_attrOffset + dataSize > size)
                throw std::runtime_error("Not enough buffer for resident list attr.");
            _data.resize(dataSize);
            memcpy(&_data[0], buf + _attrOffset, dataSize);
        }
    }
}

void AttributeList::Clear()
{
    Attribute::Clear();
    _data.clear();
}
