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

#ifndef __FILE64_H
#define __FILE64_H

#include "types.h"

class IFile64
{
public:
    static IFile64 * FileMaker();
    IFile64() : _autoClose(true) { }
    virtual ~IFile64() { }
    virtual bool Open(char const * filename) = 0;
    virtual bool Open(wchar_t const * filename) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
    virtual bool Eof() const = 0;
    virtual unsigned long Read(void * buf, unsigned long size) = 0;
    virtual bool Seek(s64 pos, u32 moveMethod=0) = 0;
    virtual s64 Size() const = 0;
    virtual void NoAutoClose() { _autoClose = false; }

protected:
    bool _autoClose;
};


#ifdef _MSC_VER

//=============================================================================
// for Win32 64bit file opening
//
class WinFile64 : public IFile64
{
public:
    WinFile64();
    WinFile64(char const * filename);
    WinFile64(wchar_t const * filename);
    ~WinFile64();

    bool Open(char const * filename);
    bool Open(wchar_t const * filename);
    void Close();
    bool IsOpen() const;
    bool Eof() const;
    unsigned long Read(void * buf, unsigned long size);
    bool Seek(s64 pos, u32 moveMethod=0); // FILE_BEGIN=0; FILE_CURRENT=1; FILE_END=2
    s64 Size() const;

private:
    void Validate() const;

    void * _h;
    bool _eof;
};

#else

//=============================================================================
// for POSIX 64bit file opening
//
class LinFile64 : public IFile64
{
public:
    LinFile64();
    LinFile64(char const * filename);
    LinFile64(wchar_t const * filename);
    ~LinFile64();

    bool Open(char const * filename);
    bool Open(wchar_t const * filename);
    void Close();
    bool IsOpen() const;
    bool Eof() const;
    unsigned long Read(void * buf, unsigned long size);
    bool Seek(s64 pos, u32 moveMethod = 0);
    s64 Size() const;

private:
    void Validate() const;

    s64 _size;
    FILE * _fp;
};

#endif // _MSC_VER

#endif // __FILE64_H
