//
// Types
// Typedef platform independent integer types.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __TYPES_H
#define __TYPES_H

#include <string>
#include <fstream>
#include <stdexcept>
#include <string.h>

#include "utf8.h"

#ifdef _MSC_VER

//-----------------------------------------------------------------------------
// Windows system
//-----------------------------------------------------------------------------

// disable warning:
//      4512 - cannot create default assignment operator
//      4127 - conditional expresssion is constant
#pragma warning(disable: 4512 4127)

#define SEPS    '\\'

typedef __int64     s64;
typedef __int32     s32;
typedef __int16     s16;
typedef __int8      s8;

typedef unsigned __int64    u64;
typedef unsigned __int32    u32;
typedef unsigned __int16    u16;
typedef unsigned __int8     u8;


#else

//-----------------------------------------------------------------------------
// POSIX system
//-----------------------------------------------------------------------------
#include <stdint.h>
#define SEPS    '/'

typedef int64_t     s64;
typedef int32_t     s32;
typedef int16_t     s16;
typedef int8_t      s8;

typedef uint64_t    u64;
typedef uint32_t    u32;
typedef uint16_t    u16;
typedef uint8_t     u8;


#endif // _MSC_VER

// compiler-correct array length retrieval
// produce compiler error if use on pointers
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
//#define ARR_LEN(x) (sizeof((x))/sizeof((x[0])))
#define ARR_LEN(x)  (sizeof(ArraySizeHelper(x)))

typedef u16     ntfschar;
typedef s64     VCN;
typedef s64     LCN;
typedef s64     LSN;

template <typename T, typename U> inline T * P_add(T * p, U n)
{
    return (T*)((u8*)p + n);
}

u32 u16len(u16 const * u16str);
s32 u16cmp(u16 const * a, u16 const * b);
void dump(char const * fname, void * buf, u32 size);

void wchar_to_utf16(std::basic_string<wchar_t> const & wstr, std::basic_string<u16> & u16str);
void wchar_to_utf8(std::basic_string<wchar_t> const & wstr, std::string & u8str);

#endif // __TYPES_H
