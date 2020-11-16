//
// Types
// Typedef platform independent integer types.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "types.h"

#ifdef _MSC_VER

//-----------------------------------------------------------------------------
// Windows system
//-----------------------------------------------------------------------------
void wchar_to_utf16(std::basic_string<wchar_t> const & wstr, std::basic_string<u16> & u16str)
{
    if (sizeof(wchar_t) != 2)   // Win32 wchar_t is 2 bytes at the moment
        throw std::runtime_error("wchar_t not 2 bytes!");
    u16str.clear();
    u16str.assign(wstr.begin(), wstr.end());
}

void wchar_to_utf8(std::basic_string<wchar_t> const & wstr, std::string & u8str)
{
    if (sizeof(wchar_t) != 2)   // Win32 wchar_t is 2 bytes at the moment
        throw std::runtime_error("wchar_t not 2 bytes!");
    u8str.clear();
    utf8::utf16to8(wstr.begin(), wstr.end(), std::back_inserter(u8str));
}

#else

//-----------------------------------------------------------------------------
// POSIX system
//-----------------------------------------------------------------------------

void wchar_to_utf16(std::basic_string<wchar_t> const & wstr, std::basic_string<u16> & u16str)
{
    if (sizeof(wchar_t) != 4)   // Linux wchar_t is 4 bytes at the moment
        throw std::runtime_error("wchar_t not 4 bytes!");
    std::string u8str;
    utf8::utf32to8(wstr.begin(), wstr.end(), std::back_inserter(u8str));
    utf8::utf8to16(u8str.begin(), u8str.end(), std::back_inserter(u16str));
}

void wchar_to_utf8(std::basic_string<wchar_t> const & wstr, std::string & u8str)
{
    if (sizeof(wchar_t) != 4)   // Linux wchar_t is 4 bytes at the moment
        throw std::runtime_error("wchar_t not 2 bytes!");
    u8str.clear();
    utf8::utf32to8(wstr.begin(), wstr.end(), std::back_inserter(u8str));
}

#endif // _MSC_VER


//-----------------------------------------------------------------------------
// generic  system
//-----------------------------------------------------------------------------
u32 u16len(u16 const * u16str)
{
    u16 const * p = u16str;
    while (*p++) ;
    return p - u16str;
}


s32 u16cmp(u16 const * a, u16 const * b)
{
    while (*a && *b && *a == *b)
    {
        ++a;
        ++b;
    }
    return *a - *b;
}


void dump(char const * fname, void * buf, u32 size)
{
    std::ofstream ofs(fname, std::ios_base::binary);
    if (!ofs)
        throw std::runtime_error("Can't dump data.");
    ofs.write((char*)buf, size);
}
