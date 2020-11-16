//
// NTFS Compress
// Compressed data stream uses variant of LZ77 that is
// simple & efficient for text/database types.
//
// This module only decompresses (inflates) the compressed
// buffer.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __NTFS_COMPRESS_H
#define __NTFS_COMPRESS_H

#include "types.h"

namespace ntfs
{
    bool decompress(u8 * dest, u32 const destSize, u8 const * src, u32 const srcSize);
}


#endif // __NTFS_COMPRESS_H
