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

#include "ntfs_compress.h"

namespace
{
    template <class T> inline u16 get_u16(T * t)
    {
        u8 * p = (u8*)t;
        return (u16)(((*p) & 0xff) | ((*(p+1) & 0xff) << 8));
    }

    //  NtfsCompressionConstants - constants used in the compression code
    enum NtfsCompressionConstants
    {
        // Compression sub-block constants
        NTFS_SB_SIZE_MASK   =   0x0fff,     // 12 bit mask for length
        NTFS_SB_SIZE        =   0x1000,     // maximum (NTFS constant) block size = 4096
        NTFS_SB_IS_COMPRESSED   =   0x8000,
    };
}


//=============================================================================
// decompress
// returns true if successful
// exception is thrown if otherwise - no false returns.
bool ntfs::decompress(u8 * dest, u32 const destSize, u8 const * src, u32 const srcSize)
{
    // main buffer limits
    u8 const * srcEnd = src + srcSize;
    u8 * destEnd = dest + destSize;

    // sub block limits
    u8 const * srcSub = 0;
    u8 const * srcSubEnd = 0;
    u8 * destSub = 0;
    u8 * destSubEnd = 0;

    // processing
    u8 const * pos = src;
    u8 tag;
    int token;

    // continue processing while we are within buffer range
    // and the header is still non-zero
    while (pos < srcEnd && dest < destEnd && get_u16(pos))
    {
        // initialize pointer for each new subblock processing
        // ** each subblock should be a logical unit of LZ77
        destSub = dest;
        destSubEnd = destSub + NTFS_SB_SIZE;

        // must not go beyond the main buffer limit
        if (destSubEnd > destEnd)
            throw std::runtime_error("Destination sub-block beyond output buffer.");

        // minimum size of compressed subblock goes beyond the main buffer limit
        // 2byte header, 1byte tag, 1byte rawToken, 2byte backrefToken
        if (pos + 6 > srcEnd)
            throw std::runtime_error("Insufficient compress data.");

        // initialize pointer for each new source sub block

        // From NTFS Document - by Richard Russon & Yuval Fledel
        // each block is preceeded by a 2-byte header:
        //      lower 12 bit is the length
        //      higher 4 bit is unknown
        //      highest 1 bit is true if block compressed
        // so decompression only OR it with 0x8000 to determine compressed or not
        //
        // the 12 bit length is stored as (n - 3)
        //      i.e. an actual length of 0x500 compressed data with 2-byte header
        //           = total size - 0x3
        //           = (0x500 + 0x2) - 0x3
        //           = 0x4ff
        srcSub = pos;
        srcSubEnd = srcSub + (get_u16(pos) & NTFS_SB_SIZE_MASK) + 3;
        if (srcSubEnd > srcEnd)
            throw std::runtime_error("Sub-block beyond compress data.");

        // process the current sub block

        if (!(get_u16(pos) & NTFS_SB_IS_COMPRESSED))
        {
            // sub block is not compress
            // skip the 2 byte header
            pos += 2;

            // insist the uncompress sub block is in full size?
            if (srcSubEnd - pos != NTFS_SB_SIZE)
                throw std::runtime_error("Uncompressed sub-block must be full size.");

            // so we are @ actual data. now copy the rest
            memcpy(dest, pos, NTFS_SB_SIZE);
            pos += NTFS_SB_SIZE;
            dest += NTFS_SB_SIZE;

            // proceed next sub block since this sub block is
            // uncompressed & we just raw copy to destination
            continue;
        }

        // if we reach here, means sub block is compressed

        // skip the 2 byte header
        pos += 2;

        // process the tag
        while (pos < srcSubEnd)
        {
            // check if we are still in range
            if (pos > srcSubEnd || dest > destSubEnd)
                throw std::runtime_error("Sub-block out of range.");

            // the tag byte is a 8 bit flag, each bit denoting whether the
            // subsequent chunk is:
            //      - zero if uncompressed (raw 1 byte) or,
            //      - one if compressed (1 byte back reference & 1 byte length)
            // e.g.
            // 1. a tag of 0b00001001 means the following stream are:
            //          [bref1,len1][a][b][bref2,len2][c][d][e][f]
            // 2. a tag of 0b00000000 means the following stream are:
            //          [a][b][c][d][e][f][g][h]

            // get the tag byte
            tag = *pos++;

            // process each token denoted by the 8bit tag flag
            for (token = 0; token < 8; ++token, tag >>= 1)
            {
                // are we done yet or still in range?
                if (pos >= srcSubEnd || dest > destSubEnd)
                   break;

                if ((tag & 0x1) == 0x0)
                {
                    // token is uncompressed so we just plain copy
                    // and continue next token
                    *dest++ = *pos++;
                    continue;
                }

                // if we have a back ref token, make sure it is not the 1st tag
                // in the sub block, because it has to refer back to the previous bytes.
                // how can it refer back if it is the first?
                if (dest == destSub)
                     throw std::runtime_error("Back ref token must not be the first.");


                // if we reach here, means the token is back reference

                // back reference example:
                // original:            "#include <ntfs.h>\n#include <stdio.h>\n"
                // original compressed: "#include <ntfs.h>[-18,10]stdio[-17,4]"
                //
                // since need 2 byte to encode back refercing, so it is useless
                // to have back ref for 2 byte stream
                //      i.e. the shortest back ref is 3 at least
                // otherwise uncompressed is better. So during encoding, 3 is subtracted.
                //
                // back referencing always never 0, so store them as positive & subtract 1.
                //
                // With all this savings, the compressed form become:
                //      "#include <ntfs.h>[17,7]stdio[16,1]"

                // back reference dynamic size reasoning:
                // given the block size is fixed maximum @ 4096, back ref might need 12 bit,
                // which left 4 bit for length - allowing maximum length of 19.
                //
                // the wastage here is if destination offset is currently @ 123, there's no way
                // the back ref can be -400 (you can't back ref beyond the big bang).
                //
                // the trick here is we can dynamically allocate the correct number of bit
                // for back ref to make way for more length bit, based on the destination offset.

                unsigned maxLengthBit = 0;
                for (unsigned i = dest - destSub - 1; // remember offset is 1 subtracted
                        i >= 0x10;  // remember 12bit max for backref or length, 4bit is the min
                        i >>= 1)
                    ++maxLengthBit;

                // get back ref token
                u16 backRefToken = get_u16(pos);
                pos += 2;

                // calculate back ref offset, by eating away length bits
                u8 * destBackRef = dest - (backRefToken >> (12 - maxLengthBit)) - 1;
                if (destBackRef < destSub)
                    throw std::runtime_error("Refer too far back.");

                // get the length
                u16 length = (backRefToken & (0xfff >> maxLengthBit)) + 3;

                // verify that we don't go beyond the buffer
                if (dest + length > destSubEnd)
                    throw std::runtime_error("Output buffer too small.");

                // now copy the back ref to current destination
                // it is also possible to have overlapped copy
                while (length--)
                    *dest++ = *destBackRef++;

            } // for each token or for each tag bit
        } // while we haven't finish the sub block, continue next tag

        // if destination sub block not full length
        if (dest < destSubEnd)
        {
            int zerobytes = destSubEnd - dest;
            memset(dest, 0, zerobytes);
            dest += zerobytes;
        }

    } // next sub block

    return true;
}
