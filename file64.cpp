//
// 64bit File Reading
// A factory method returning either Win32 or Linux file
// reading object. Both Win32 & Linux definition are
// conditionally preprocessed depends on compiler platforms.
//
// Based on the _MSC_VER symbol, if defined means Win32,
// otherwise Linux.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "file64.h"

#include <stdio.h>
#include <stdexcept>
#include <assert.h>

IFile64 * IFile64::FileMaker()
{

#ifdef _MSC_VER
    return new WinFile64;
#else
    return new LinFile64;
#endif

}


// if using Microsoft Visual Studio compiler
// We can safely assume Win32 API exists
#ifdef _MSC_VER

#include <windows.h>
//=============================================================================
// for Win32 64bit file opening
WinFile64::WinFile64() : _h(INVALID_HANDLE_VALUE) { }
WinFile64::WinFile64(char const * filename) : _h(INVALID_HANDLE_VALUE) { Open(filename); }
WinFile64::WinFile64(wchar_t const * filename) : _h(INVALID_HANDLE_VALUE) { Open(filename); }
WinFile64::~WinFile64() { if (_autoClose) Close(); }
bool WinFile64::Open(char const * filename)
{
    _h = ::CreateFileA(filename, FILE_READ_DATA, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    _eof = false;
    return IsOpen();
}
bool WinFile64::Open(wchar_t const * filename)
{
    _h = ::CreateFileW(filename, FILE_READ_DATA, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    _eof = false;
    return IsOpen();
}
bool WinFile64::IsOpen() const { return _h != INVALID_HANDLE_VALUE && _h; }
bool WinFile64::Eof() const { return !IsOpen() || _eof; }
void WinFile64::Close()
{
    if (_h && INVALID_HANDLE_VALUE != _h)
    {
        ::CloseHandle(_h);
        _h = 0;
    }
}
unsigned long WinFile64::Read(void * buf, unsigned long size)
{
    Validate();
    DWORD dw=0;
    BOOL res = ::ReadFile(_h, buf, size, &dw, 0);
    _eof = (res && dw == 0);
    return dw;
}
bool WinFile64::Seek(s64 pos, u32 moveMethod)
{
    Validate();
    LARGE_INTEGER li;
    li.QuadPart = pos;
    BOOL res = ::SetFilePointerEx(_h, li, 0, moveMethod);
    return res != 0;
}
s64 WinFile64::Size() const
{
    Validate();
    LARGE_INTEGER li;
    BOOL res = ::GetFileSizeEx(_h, &li);
    return res ? li.QuadPart : -1;
}
void WinFile64::Validate() const
{
    if (_h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("File not open.");
}

#else

#include <stdio.h>
//=============================================================================
// for POSIX 64bit file opening
//
LinFile64::LinFile64() : _fp(0) { }
LinFile64::LinFile64(char const * filename) : _fp(0) { Open(filename); }
LinFile64::LinFile64(wchar_t const * filename) : _fp(0) { Open(filename); }
LinFile64::~LinFile64() { if (_autoClose) Close(); }
bool LinFile64::Open(char const * filename)
{
    _fp = fopen64(filename, "rb");
    return IsOpen();
}
bool LinFile64::Open(wchar_t const * /*filename*/)
{
    throw std::runtime_error("Opening wide char filename not supported in Linux.");
    //return IsOpen();
}
bool LinFile64::IsOpen() const { return _fp != 0; }
bool LinFile64::Eof() const { return !IsOpen() || feof(_fp); }
void LinFile64::Close() { if (_fp) fclose(_fp); _fp = 0; }
unsigned long LinFile64::Read(void * buf, unsigned long size)
{
    Validate();
    return fread(buf, sizeof(char), size, _fp);
}
bool LinFile64::Seek(s64 pos, u32 moveMethod)
{
    //static const int method[] = { SEEK_SET, SEEK_CUR, SEEK_END };
    Validate();
    return fseeko64(_fp, pos, moveMethod) == 0;
}
s64 LinFile64::Size() const
{
    Validate();
    //off_t oldpos = ftello64(_fp);
    fseeko64(_fp, 0, SEEK_END);
    s64 size = ftello64(_fp);
    //fseeko64(_fp, oldpos, SEEK_SET);
    fseeko64(_fp, 0, SEEK_SET);
    return size;
}
void LinFile64::Validate() const
{
    if (!_fp)
        throw std::runtime_error("File not open.");
}

#endif // _MSC_VER
