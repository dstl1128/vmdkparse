//
// NTFS File
// Base on the result of ntfs::Tree, provides generic 64bit file
// reading interfaces with Win32-like path target, and reads
// the file content from the NTFS file system.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "ntfs_file.h"
#include "ntfs_compress.h"
#include "stringtok.h"
#include "utf8.h"
#include <string>

using namespace ntfs;

//=============================================================================
File::File(ntfs::Tree & tree)
: _tree(tree), _ntfs(_tree._ntfs), _pos(~0ULL), _clustersPerGroup(0), _oldClusterNumber(~0ULL)
{
    _clusterBuf.resize(_ntfs.GetBytesPerSector() * _ntfs.GetSectorsPerCluster());
}

bool File::Open(char const * filename)
{
    std::basic_string<u16> utf16Filename;
    std::string utf8Filename(filename);
    std::string::iterator endIt = utf8::find_invalid(utf8Filename.begin(), utf8Filename.end());
    if (endIt != utf8Filename.end())
        throw std::runtime_error("Invalid utf8 filename.");
    utf8::utf8to16(utf8Filename.begin(), endIt, std::back_inserter(utf16Filename));
    return OpenInternal(utf16Filename.c_str());
}

bool File::Open(wchar_t const * filename)
{
    std::basic_string<u16> utf16Filename;
    std::wstring wstr(filename);
    wchar_to_utf16(wstr, utf16Filename);
    return OpenInternal(utf16Filename.c_str());
}

bool File::OpenInternal(std::basic_string<u16> const & filename)
{
    u16 const s_seps[] = { '\\', '/', 0 };
    typedef std::basic_string<u16> U16STR;
    U16STR token, streamName;
    u64 folderMft = 5; // start with root (mft=5)
    bool found = false;

    StringTok<U16STR> stoken(filename);
    for (token = stoken(s_seps);
        !token.empty() || folderMft != 0; // OR condition because monkey input might give "/WINDOWS/System32/notepad.exe/wtfinvalid"
        token = stoken(s_seps))
    {
        U16STR::size_type pos = token.find_first_of(':');
        if (pos != U16STR::npos)
        {
            streamName = token.substr(pos+1);
            token = token.substr(0, pos);
        }
        else
            streamName.clear();

        ntfs::FOLDERS::iterator it = _tree._folders.find(folderMft);
        if (it == _tree._folders.end())
            throw std::runtime_error("Can't find MFT entry.");

        // search for node via name
        found = false;
        ntfs::NODES::iterator nit = it->second.begin();
        for (; nit != it->second.end(); ++nit)
        {
            if (nit->name.compare(token) == 0 || nit->shortname.compare(token) == 0)
            {
                if (nit->isdir)
                {
                    folderMft = nit->mftRef;
                }
                else
                {
                    _node = *nit;
                    folderMft = 0;
                }
                found = true;
                break;
            }
        }
        if (!found)
            throw std::runtime_error("Can't find full path name.");
    }

    // check for valid stream
    if (found)
    {
        ntfs::STREAMS::iterator it = _node.streams.find(streamName);
        if (it == _node.streams.end())
            throw std::runtime_error("Cannot find stream name.");
        _stream = it->second;
        _pos = 0;
        _oldClusterNumber = ~0ULL;
        if (_stream.compressed)
        {
            _clustersPerGroup = 1 << _stream.compressUnitSize;
            _clusterBuf.resize(_ntfs.GetBytesPerSector() * _ntfs.GetSectorsPerCluster() * _clustersPerGroup);
            _compressBuf.resize(_ntfs.GetBytesPerSector() * _ntfs.GetSectorsPerCluster() * _clustersPerGroup);
            //_posCompress = 0;
        }

        //printf("Found size: %llu\n", _stream.realSize);
    }
    return IsOpen();
}

void File::Close()
{
    _stream.Clear();
    _node.Clear();
}

bool File::IsOpen() const
{
    return !_stream.IsEmpty() && !_node.IsEmpty();
}

bool File::Eof() const
{
    return _pos >= _stream.realSize || !IsOpen();
}

unsigned long File::Read(void * buf, unsigned long size)
{
    Validate();
    unsigned long bytesRead = 0;
    if (_stream.nonResident)
    {
        if (_stream.compressed)
        {
            // ntfs dictates cluster group size must be 16, and cluster size be 4096 bytes (4096=12bits)
            // otherwise compression doesn't work -> they use 12bits LZ77 variant
            // ie. group size == 16 ==> 64k per compression block
            if (_clustersPerGroup != 16)
                throw std::runtime_error("Unsupported compression block size");
            if (_compressBuf.size() != _clusterBuf.size())
                throw std::runtime_error("Compression buffer not in sync.");

            unsigned long clusterSize = _ntfs.GetBytesPerSector() * _ntfs.GetSectorsPerCluster();
            unsigned long clusterGroupSize = _clusterBuf.size();

            while (bytesRead < size && _pos < _stream.realSize)
            {
                // get cluster group number
                u64 vcgn = _pos / clusterGroupSize;
                u64 vcnStart = vcgn * _clustersPerGroup;

                // do decompression if we haven't process this cluster group
                if (vcnStart != _oldClusterNumber)
                {
                    // read the correct compressed cluster group (for decompression)
                    u64 vcnEnd = vcnStart + _clustersPerGroup;
                    u32 count = 0;
                    u32 clusterGroupMap = 0;
                    int flags=0;  // 0=sparse; 1=group compressed; 2=group uncompressed
                    for (u64 vcn = vcnStart; vcn < vcnEnd; ++vcn, ++count)
                    {
                        u64 lcn = _stream.dataRun.Vcn2Lcn(vcn);

                        // clusterGroupMap is a bitmap of 16 cluster (hence 16bit) for taking note of used clusters
                        // bit of 1 means cluster is in used
                        // bit of 0 means cluster unused
                        // if all bits are set, i.e. 16 clusters are fully occupied, then this group is uncompressed
                        // if no bits are set, i.e. no clusters are used, then this is a sparse group
                        // else, this group is compressed
                        clusterGroupMap |= ((lcn == 0) ? 0 : (1 << count));

                        if (vcnStart != _oldClusterNumber)
                        {
                            // only cache disk i/o
                            if (lcn)
                                _ntfs.ReadLCN(lcn, 1, &_compressBuf[count * clusterSize]);
                            else
                                memset(&_compressBuf[count * clusterSize], 0, sizeof(clusterSize));
                        }
                    }
                    flags = ((clusterGroupMap & 0xffff) == 0xffff) ? 2 : (clusterGroupMap==0?0:1);

                    // starts decompression
                    switch (flags)
                    {
                    case 0: // sparse
                        memset(&_clusterBuf[0], 0, _clusterBuf.size());
                        break;
                    case 1: // group compressed
                        if (!ntfs::decompress(&_clusterBuf[0], _clusterBuf.size(), &_compressBuf[0], _compressBuf.size()))
                            throw std::runtime_error("Unable to decompress");
                        break;
                    case 2: // group uncompressed
                        memcpy(&_clusterBuf[0], &_compressBuf[0], _clusterBuf.size());
                        break;
                    default:
                        throw std::runtime_error("unknown flag");
                    }

                    _oldClusterNumber = vcnStart;
                }

                // copies decompressed data to output buffer
                unsigned long offset = (unsigned long)(_pos % clusterGroupSize);
                unsigned long len = (unsigned long)std::min<u64>(_stream.realSize - _pos, clusterGroupSize - offset);
                len = std::min(len, size);
                memcpy(buf, &_clusterBuf[offset], len);

                _pos += len;
                buf = P_add(buf, len);
                bytesRead += len;
            }
        }
        else
        {
            // uncompress stream
            unsigned long clusterSize = _clusterBuf.size();
            u8 * pbytes = (u8*) buf;
            while (bytesRead < size && _pos < _stream.realSize)
            {
                u64 vcn = _pos / clusterSize;
                u64 lcn = _stream.dataRun.Vcn2Lcn(vcn);
                unsigned long bytesOffset = (unsigned long)(_pos % clusterSize);
                unsigned long len = (unsigned long)std::min<u64>(clusterSize - bytesOffset, _stream.realSize - _pos);
                len = std::min(len, size);

                if (lcn > 0 && lcn != _oldClusterNumber)
                {
                    // minor caching to avoid re-reading the same cluster over & over again
                    _ntfs.ReadLCN(lcn, 1, &_clusterBuf[0]);
                    _oldClusterNumber = lcn;
                }
                else if (lcn == 0)
                {
                    // unused sparse cluster reached - zeroes the buffer
                    memset(&_clusterBuf[0], 0, _clusterBuf.size());
                    _oldClusterNumber = lcn;
                }

                memcpy(pbytes, &_clusterBuf[bytesOffset], len);
                bytesRead += len;
                _pos += len;
                pbytes += len;
            }
        }
    }
    else
    {
        // resident data shouldn't be larger than 32bit size - cast should be safe.
        unsigned long len = (unsigned long)std::min<u64>(_stream.realSize - _pos, size);
        memcpy(buf, &_stream.data[(unsigned long)_pos], len);
        _pos += len;
        bytesRead += len;
    }
    return bytesRead;
}

bool File::Seek(s64 pos, u32 moveMethod)
{
    Validate();
    u64 newpos;
    switch (moveMethod)
    {
    case 0: // FILE_BEGIN
        newpos = pos;
        break;
    case 1: // FILE_CURRENT
        newpos = _pos + pos;
        break;
    case 2: // FILE_END
        newpos = _stream.realSize - pos;
        break;
    default:
        throw std::runtime_error("Invalid ntfs file seek method.");
    }
    if (newpos > _stream.realSize)
        return false;
    _pos = newpos;
    return true;
}

s64 File::Size() const
{
    return _stream.realSize;
}

void File::Validate() const
{
    if (!IsOpen())
        throw std::runtime_error("Ntfs file not opened yet.");
}
