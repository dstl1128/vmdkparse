//
// NTFS Layout
// Defines the NTFS attributes structure
// as well as various MFT entry and header structure.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __NTFS_LAYOUT_H
#define __NTFS_LAYOUT_H

#include "types.h"

namespace ntfs
{

#pragma pack(push, 1)
    typedef struct BOOT_BLOCK {
        u8 Jump[3];                     // 0x0
        u8 Format[8];                   // 0x3

        // boot param block start
        u16 BytesPerSector;          // 0xb
        u8 SectorsPerCluster;        // 0xd
        u16 BootSectors;             // 0xe - zero (FAT use?)
        u8 Fats;                     // 0x10 - Zero (FAT use?)
        u16 RootEntries;             // 0x11 - Zero (FAT use?)
        u16 Sectors;                 // 0x13 - Zero (FAT use?)
        u8 MediaType;                // 0x15 - 0xf8 = harddisk
        u16 SectorsPerFat;           // 0x16 - sector per fat (FAT use?)
        u16 SectorsPerTrack;         // 0x18
        u16 NumberOfHeads;           // 0x1a
        u32 PartitionOffset;         // 0x1c - start of partition
        u32 LargeSector;             // 0x20 - zero
        // boot param block end

        u8 PhysicalDrive;            // 0x24 - 0x00=floppy; 0x80=harddisk
        u8 CurrentHead;              // 0x25
        u8 ExtendedBootSignature;    // 0x26 - 0x80
        u8 _reserved1;               // 0x27
        u64 TotalSectors;            // 0x28
        u64 MftStartLcn;             // 0x30
        u64 Mft2StartLcn;            // 0x38
        u8 ClustersPerFileRecord;    // 0x40
        u8 _reserved2[3];
        u8 ClustersPerIndexBlock;    // 0x44
        u8 _reserved3[3];
        u64 VolumeSerialNumber;      // 0x48
        u32 Checksum;                // 0x50
        u8 Code[426];                // 0x54
        u16 BootSignature;           // 0x1fe - always 0xaa55 little endian
    } BOOT_BLOCK, *PBOOT_BLOCK;         // 0x200


    typedef struct NTFS_FILE_RECORD_INPUT_BUFFER {
        u64 FileReferenceNumber;
    } NTFS_FILE_RECORD_INPUT_BUFFER, *PNTFS_FILE_RECORD_INPUT_BUFFER;

    typedef struct NTFS_FILE_RECORD_OUTPUT_BUFFER {
        u64 FileReferenceNumber;
        u32 FileRecordLength;
        u8 FileRecordBuffer[1];
    } NTFS_FILE_RECORD_OUTPUT_BUFFER, *PNTFS_FILE_RECORD_OUTPUT_BUFFER;


    enum NTFS_RECORD_TYPE
    {
        // found in $MFT/$DATA
        magic_FILE  =   0x454c4946,     // Mft entry
        magic_INDX  =   0x58444e49,     // Index buffer
        magic_HOLE  =   0x454c4f48,     // ?

        // found in $Logfile/$DATA
        magic_RSTR  =   0x52545352,     // restart page
        magic_RCRD  =   0x44524352,     // log record page

        // found in $LogFile/$DATA (maybe found in $MFT/Data$ too?)
        magic_CHKD  =   0x444b4843,     // modified by chkdsk

        // found in all ntfs record containing records
        magic_BAAD  =   0x44414142,     // failed multi sector transfer detected

        magic_empty =   0xffffffff      // record is empty & has to be init
    };

    enum NTFS_RECORD_FLAGS
    {
        MFT_RECORD_IN_USE           = 0x0001,
        MFT_RECORD_IS_DIRECTORY     = 0x0002,
        MFT_RECORD_IS_4             = 0x0004,
        MFT_RECORD_IS_VIEW_INDEX    = 0x0008,
        MFT_REC_SPACE_FILLER        = 0xffff
    };

    typedef struct NTFS_RECORD_HEADER {
        u32 Type;               // 0x0
        u16 UsaOffset;          // 0x4
        u16 UsaCount;           // 0x6
        s64 Usn;                // 0x8
    } NTFS_RECORD_HEADER, *PNTFS_RECORD_HEADER; // 0x10


    typedef struct FILE_RECORD_HEADER {
        NTFS_RECORD_HEADER Ntfs;     // 0x0
        u16 SequenceNumber;          // 0x10
        u16 LinkCount;               // 0x12
        u16 AttributesOffset;        // 0x14
        u16 Flags;                   // 0x16 - 0x1=InUse; 0x2=Directory
        u32 BytesInUse;              // 0x18
        u32 BytesAllocated;          // 0x1c
        u64 BaseFileRecord;          // 0x20
        u16 NextAttributeNumber;     // 0x28
    } FILE_RECORD_HEADER, *PFILE_RECORD_HEADER; //0x30

    typedef struct STANDARD_INFORMATION {
        u64 CreationTime;
        u64 ChangeTime;
        u64 LastWriteTime;
        u64 LastAccessTime;
        u32 FileAttributes;
        u32 AlignmentOrReservedOrUnknown[3];
        u32 QuotaId; // NTFS 3.0 only
        u32 SecurityId; // NTFS 3.0 only
        u64 QuotaCharge; // NTFS 3.0 only
        s64 Usn; // NTFS 3.0 only
    } STANDARD_INFORMATION, *PSTANDARD_INFORMATION;


    typedef enum ATTRIBUTE_TYPE {
        eAttributeStandardInformation   = 0x10,
        eAttributeAttributeList         = 0x20,
        eAttributeFileName              = 0x30,
        eAttributeObjectId              = 0x40,
        eAttributeSecurityDescriptor    = 0x50,
        eAttributeVolumeName            = 0x60,
        eAttributeVolumeInformation     = 0x70,
        eAttributeData                  = 0x80,
        eAttributeIndexRoot             = 0x90,
        eAttributeIndexAllocation       = 0xA0,
        eAttributeBitmap                = 0xB0,
        eAttributeReparsePoint          = 0xC0,
        eAttributeEAInformation         = 0xD0,
        eAttributeEA                    = 0xE0,
        eAttributePropertySet           = 0xF0,
        eAttributeLoggedUtilityStream   = 0x100,
        eAttributeTerminator            = 0xFFFFFFFF
    } ATTRIBUTE_TYPE, *PATTRIBUTE_TYPE;

    char const * attrtype2str(ATTRIBUTE_TYPE t);

    typedef struct ATTRIBUTE {
        ATTRIBUTE_TYPE AttributeType;   // 0x0
        u32 Length;                  // 0x4
        u8 Nonresident;              // 0x8
        u8 NameLength;               // 0x9
        u16 NameOffset;              // 0xa
        u16 Flags;                   // 0xc      // 0x0001 = Compressed
        u16 AttributeNumber;         // 0xe
    } ATTRIBUTE, *PATTRIBUTE;           // 0x10


    typedef struct RESIDENT_ATTRIBUTE {
        ATTRIBUTE Attribute;        // 0x0
        u32 ValueLength;            // 0x10
        u16 ValueOffset;            // 0x14
        u16 ResidentFlags;          // 0x16     // 0x0001 = Indexed
    } RESIDENT_ATTRIBUTE, *PRESIDENT_ATTRIBUTE; // 0x18


    typedef struct NONRESIDENT_ATTRIBUTE {
        ATTRIBUTE Attribute;            // 0x0
        u64 StartVcn;                // 0x10
        u64 LastVcn;                 // 0x18
        u16 DataRunOffset;           // 0x20
        u16 CompressionUnitSize;     // 0x22
        u32 Padding;                 // 0x24
        u64 AllocatedSize;           // 0x28
        u64 RealSize;                // 0x30
        u64 InitializedDatSize;      // 0x38
        //u64 CompressedSize;          // 0x40 if compressed
    } NONRESIDENT_ATTRIBUTE, *PNONRESIDENT_ATTRIBUTE; // 0x40


    typedef struct ATTRIBUTE_LIST {
        ATTRIBUTE_TYPE AttributeType;   // 0x0
        u16 Length;                  // 0x4
        u8 NameLength;               // 0x6
        u8 NameOffset;               // 0x7
        u64 LowVcn;                  // 0x8 start vcn
        u64 FileReferenceNumber;     // 0x10 mft ref; 1st 48bit is mft index number; last 16bit is sequence number
        u16 AttributeNumber;         // 0x18 attr id
        u16 AlignmentOrReserved[3];  // 0x1a
    } ATTRIBUTE_LIST, *PATTRIBUTE_LIST; // 0x20

    typedef struct FILENAME_ATTRIBUTE {
        u64 DirectoryFileReferenceNumber;   // 0x0
        u64 CreationTime;                   // 0x08 // Saved when filename last changed
        u64 ChangeTime;                     // 0x10 // ditto
        u64 LastWriteTime;                  // 0x18 // ditto
        u64 LastAccessTime;                 // 0x20 // ditto
        u64 AllocatedSize;                  // 0x28 // ditto
        u64 DataSize;                       // 0x30 // ditto
        u32 FileAttributes;                 // 0x38 // ditto
        u32 AlignmentOrReserved;            // 0x3c
        u8 NameLength;                      // 0x40
        u8 NameType;                        // 0x41 // 0x0=POSIX; 0x01=Win32 Long; 0x02=DOS Short; 0x03=Win32 & DOS same name
        u16 Name[1];                        // 0x42 // wchar_t
    } FILENAME_ATTRIBUTE, *PFILENAME_ATTRIBUTE;

    typedef struct _GUID {
        unsigned long  Data1;
        unsigned short Data2;
        unsigned short Data3;
        unsigned char  Data4[ 8 ];
    } GUID;

    typedef struct OBJECTID_ATTRIBUTE {
        GUID ObjectId;
        union dummy1 {
            struct dummy2 {
                GUID BirthVolumeId;
                GUID BirthObjectId;
                GUID DomainId;
            } ;
            u8 ExtendedInfo[48];
        };
    } OBJECTID_ATTRIBUTE, *POBJECTID_ATTRIBUTE;


    typedef struct VOLUME_INFORMATION {
        u32 Unknown[2];
        u8 MajorVersion;
        u8 MinorVersion;
        u16 Flags;
    } VOLUME_INFORMATION, *PVOLUME_INFORMATION;

    typedef struct DIRECTORY_INDEX {
        u32 EntriesOffset;
        u32 IndexBlockLength;
        u32 AllocatedSize;
        u32 Flags; // 0x00 = Small directory, 0x01 = Large directory
    } DIRECTORY_INDEX, *PDIRECTORY_INDEX;

    typedef struct INDEX_ROOT {
        ATTRIBUTE_TYPE Type;
        u32 CollationRule;
        u32 BytesPerIndexBlock;
        u32 ClustersPerIndexBlock;
        DIRECTORY_INDEX DirectoryIndex;
    } INDEX_ROOT, *PINDEX_ROOT;

    typedef struct INDEX_BLOCK_HEADER {
        NTFS_RECORD_HEADER Ntfs;
        u64 IndexBlockVcn;
        DIRECTORY_INDEX DirectoryIndex;
    } INDEX_BLOCK_HEADER, *PINDEX_BLOCK_HEADER;

    typedef struct DIRECTORY_ENTRY {
        u64 FileReferenceNumber;
        u16 Length;
        u16 AttributeLength;
        u32 Flags; // 0x01 = Has trailing VCN, 0x02 = Last entry
        FILENAME_ATTRIBUTE FName;
        // u64 Vcn; // VCN in IndexAllocation of earlier entries
    } DIRECTORY_ENTRY, *PDIRECTORY_ENTRY;

    typedef struct REPARSE_POINT {
        u32 ReparseTag;
        u16 ReparseDataLength;
        u16 Reserved;
        u8 ReparseData[1];
    } REPARSE_POINT, *PREPARSE_POINT;

    typedef struct EA_INFORMATION {
        u32 EaLength;
        u32 EaQueryLength;
    } EA_INFORMATION, *PEA_INFORMATION;

    typedef struct EA_ATTRIBUTE {
        u32 NextEntryOffset;
        u8 Flags;
        u8 EaNameLength;
        u16 EaValueLength;
        u8 EaName[1]; // char
        // u8 EaData[];
    } EA_ATTRIBUTE, *PEA_ATTRIBUTE;

    typedef struct ATTRIBUTE_DEFINITION {
        u16 AttributeName[64]; // wchar_t
        u32 AttributeNumber;
        u32 Unknown[2];
        u32 Flags;
        u64 MinimumSize;
        u64 MaximumSize;
    } ATTRIBUTE_DEFINITION, *PATTRIBUTE_DEFINITION;

#pragma pack(pop)


}

#endif // __NTFS_LAYOUT_H
