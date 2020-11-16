//
// NTFS Layout
// Defines the NTFS attributes structure
// as well as various MFT entry and header structure.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "ntfs_layout.h"

using namespace ntfs;

struct Enum2Str
{
    u32 id;
    const char * name;
};

Enum2Str g_attrtype[] =
{
    {eAttributeStandardInformation, "Standard Information"},
    {eAttributeAttributeList, "Attribute List"},
    {eAttributeFileName, "File Name"},
    {eAttributeObjectId, "Object Id"},
    {eAttributeSecurityDescriptor, "Security Descriptor"},
    {eAttributeVolumeName, "Volume Name"},
    {eAttributeVolumeInformation, "Volume Information"},
    {eAttributeData, "Data"},
    {eAttributeIndexRoot, "Index Root"},
    {eAttributeIndexAllocation, "Index Allocation"},
    {eAttributeBitmap, "Bitmap"},
    {eAttributeReparsePoint, "Reparse Point"},
    {eAttributeEAInformation, "EA Information"},
    {eAttributeEA, "EA"},
    {eAttributePropertySet, "Property Set"},
    {eAttributeLoggedUtilityStream, "Logged Utility Stream"},
    {static_cast<u32>(eAttributeTerminator), "Terminator"}
};


char const * ntfs::attrtype2str(ATTRIBUTE_TYPE t)
{
    for (unsigned i = 0; i < ARR_LEN(g_attrtype); ++i)
    {
        if (g_attrtype[i].id == static_cast<u32>(t))
            return g_attrtype[i].name;
    }
    return "???";
}
