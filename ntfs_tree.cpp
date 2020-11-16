//
// NTFS Tree
// Parses the entire $MFT file and
// reconstructs files and folders hierarchy.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "ntfs_tree.h"

#include "utf8.h"


using namespace ntfs;

Tree::Tree(ntfs::Ntfs & ntfs)
: _ntfs(ntfs)
{
    Init();
}


void Tree::Init()
{
    // dump full MFT data
    //{
    //    std::vector<u8> buf(_ntfs.GetBytesPerSector() * _ntfs.GetSectorsPerCluster());
    //    std::ofstream ofs("mft_full.bin", std::ios_base::binary);
    //    for (u64 s = _ntfs.GetMftStartVcn(); s < _ntfs.GetMftEndVcn(); ++s)
    //    {
    //        u64 lcn = _ntfs._pMftDataRun->Vcn2Lcn(s);
    //        _ntfs.ReadLCN(lcn, 1, &buf[0]);
    //        ofs.write((char*)&buf[0], buf.size());
    //    }
    //}

    // iterate each record in MFT
    u64 n = _ntfs.GetMftSize() / _ntfs.GetFileRecordSize();
    if (n > MFT_MASK)
        throw std::runtime_error("Too much MFT entries.");

    std::vector<u8> buf(_ntfs.GetFileRecordSize());
    ntfs::FILE_RECORD_HEADER * phdr = (ntfs::FILE_RECORD_HEADER*)&buf[0];
    ntfs::FILE_RECORD_HEADER * phdrEnd = (ntfs::FILE_RECORD_HEADER*)P_add(&buf[0], buf.size());
    u64 count = 0;
    for(u64 i = 16; i < n; ++i)     // 16 is the 1st non-special file records
    {
        _ntfs.ReadFileRecord(i, phdr);
        if (phdr->Ntfs.Type != magic_FILE || !(phdr->Flags & 0x3))
            continue;

        // create node
        ntfs::Node node;
        node.mftRef = i; //phdr->BaseFileRecord & MFT_MASK;
        node.isdir = ((phdr->Flags & 0x2) == 0x2);
        if (node.isdir)
            _folders.insert(std::make_pair(node.mftRef, NODES()));

        // iterate each attr
        ProcessAttribute(
            (ntfs::ATTRIBUTE*)P_add(phdr, phdr->AttributesOffset),
            (ntfs::ATTRIBUTE*)phdrEnd,
            node);

        // ignore parentRef=0 entry:
        //   - probably is reserved entry or,
        //   - is an attribute list extended from other MFT entry
        // or ignore system reserved entry (i < 16)
        if (node.parentRef == 0 || node.mftRef == 5 || i < 16)
            continue;

        // adds to folders container
        ntfs::FOLDERS::iterator it;
        it = _folders.find(node.parentRef);
        if (it == _folders.end())
        {
            NODES nodes;
            nodes.push_back(node);
            _folders.insert(std::make_pair(node.parentRef, nodes));
        }
        else
        {
            it->second.push_back(node);
        }
        ++count;
    }
    //printf("Total file records: %llu\n", n);
    //printf("In used file records: %llu\n", count);
    //printf("Total folder: %lu\n", _folders.size());

    // verify root folder must exists
    if (_folders.find(5) == _folders.end())
        throw std::runtime_error("Missing root folders.");
}

void Tree::ProcessAttribute(ATTRIBUTE * pattr, ATTRIBUTE * pattrEnd, ntfs::Node & node, u64 listref, u16 attrNum)
{
    // iterate each attr
    for (; pattr->AttributeType != eAttributeTerminator && pattr < pattrEnd; pattr = P_add(pattr, pattr->Length))
    {
        switch (pattr->AttributeType)
        {
        case eAttributeFileName:
            {
                if (pattr->Nonresident)
                    throw std::runtime_error("WTF! Non resident file name attribute.");

                ntfs::RESIDENT_ATTRIBUTE * prattr = (RESIDENT_ATTRIBUTE*)pattr;
                ntfs::FILENAME_ATTRIBUTE * pfa = (ntfs::FILENAME_ATTRIBUTE*)P_add(pattr, prattr->ValueOffset);
                void * p = P_add(pfa, prattr->ValueLength);
                node.attr = pfa->FileAttributes;
                node.parentRef = pfa->DirectoryFileReferenceNumber & MFT_MASK;
                if (pfa->NameType & 0x2)
                {
                    if ((pfa->Name + pfa->NameLength) > p)
                        throw std::runtime_error("Out of range name reading.");
                    node.shortname.assign(pfa->Name, pfa->Name + pfa->NameLength);
                }
                if (pfa->NameType & 0x1)
                {
                    if ((pfa->Name + pfa->NameLength) > p)
                        throw std::runtime_error("Out of range name reading.");
                    node.name.assign(pfa->Name, pfa->Name + pfa->NameLength);
                }
                _parentMap[node.mftRef] = node.parentRef;
            }
            break;

        case eAttributeData:
            {
                ntfs::AttributeData attr;
                attr.Init((u8*)pattr, pattr->Length);

                Stream s;
                s.name = attr._attrName;
                s.nonResident = attr._nonResident;
                s.realSize = attr.GetDataLength();
                s.compressed = ((attr._flags & 0x1) == 1);
                s.compressSize = attr._compressSize;
                s.compressUnitSize = attr._compressionUnitSize;
                s.data.swap(attr._data);

                STREAMS::iterator it = node.streams.find(s.name);
                if (it == node.streams.end())
                {
                    if (s.nonResident)
                    {
                        // if non resident then parse the datarun
                        s.dataRun.Init(&s.data[0], s.data.size(), attr._startVcn);
                        s.data.clear();
                    }
                    node.streams.insert(std::make_pair(s.name, s));
                }
                else
                {
                    if (s.nonResident != it->second.nonResident)
                        throw std::runtime_error("Different residency under same data stream");
                    if (listref != 0)
                    {
                        // must be super fragmented and is recursive from attribute-list
                        // so we extent the existing DataRun for this data stream
                        if (it->second.dataRun.Empty())
                            throw std::runtime_error("DataRun shouldn't empty if dataRun extension needed.");
                        if (attrNum == attr._attrId)
                            it->second.dataRun.Append(&s.data[0], s.data.size(), attr._startVcn);
                        s.data.clear();
                    }
                    else
                    {
                        if (!std::equal(s.data.begin(), s.data.end(), it->second.data.begin()))
                            throw std::runtime_error("Data not the same.");
                    }
                }
            }
            break;

        case eAttributeAttributeList:
            {
                // if we keep coming back as a dead-loop
                // just stop right here
                if (listref != 0)
                    break;

                ntfs::AttributeList attrlist;
                attrlist.Init((u8*)pattr, pattr->Length);
                if (attrlist._nonResident)
                {
                    throw std::runtime_error("Still don't know how to do non resident attr list.");
                }
                else
                {
                    std::vector<u8> buf(_ntfs.GetFileRecordSize());
                    ntfs::FILE_RECORD_HEADER * phdr = (ntfs::FILE_RECORD_HEADER*)&buf[0];
                    ATTRIBUTE_LIST * pListEntry = (ATTRIBUTE_LIST*)&attrlist._data[0];
                    ATTRIBUTE_LIST * pListEntryEnd = P_add(pListEntry, attrlist._data.size());
                    for (; pListEntry < pListEntryEnd && pListEntry->Length > 0;
                            pListEntry = P_add(pListEntry, pListEntry->Length))
                    {
                        if (!(pListEntry->FileReferenceNumber & MFT_MASK))  // skip if is $MFT entry (#0 entry in MFT)
                            continue;

                        _ntfs.ReadFileRecord(pListEntry->FileReferenceNumber, phdr);
                        if (buf[0] != 'F' || buf[1] != 'I' || buf[2] != 'L' || buf[3] != 'E')   // skip non file record
                            continue;
                        if (!(phdr->Flags & 0x01)) // skip not in used entry
                            continue;

                        ProcessAttribute(
                            (ntfs::ATTRIBUTE*)P_add(phdr, phdr->AttributesOffset),
                            (ntfs::ATTRIBUTE*)P_add(&buf[0], buf.size()),
                            node,
                            pListEntry->FileReferenceNumber & MFT_MASK,
                            pListEntry->AttributeNumber);
                    }
                }
            }
            break;

        default:
            break;
        }
    }

}

void Tree::Print(char const * prefixDir, std::ostream & os, u64 folderMftIndex)
{
    PrintInternal(prefixDir, os, folderMftIndex);
}

void Tree::Print(wchar_t const * baseDir, std::ostream & os, u64 folderMftIndex)
{
    std::basic_string<wchar_t> wstr(baseDir);
    std::string u8dir;
    wchar_to_utf8(wstr, u8dir);
    PrintInternal(u8dir, os, folderMftIndex);
}

void Tree::PrintInternal(std::string const & prefixDir, std::ostream & os, u64 folderMftIndex)
{
    ntfs::FOLDERS::iterator it;
    it = _folders.find(folderMftIndex);
    if (it == _folders.end())
        throw std::runtime_error("Can't find folder with the given MFT index.");

    os << prefixDir;
    if (prefixDir.length() < 3 && *prefixDir.rbegin() != '\\')
        os << '\\';
    os << std::endl;

    std::string u8name;
    std::string u8stream;
    ntfs::NODES::iterator nit;
    for (nit = it->second.begin(); nit != it->second.end(); ++nit)
    {
        if (!nit->isdir)
        {
            ntfs::STREAMS::iterator sit;

            for (sit = nit->streams.begin(); sit != nit->streams.end(); ++sit)
            {
                u8name.clear();
                utf8::utf16to8(nit->name.begin(), nit->name.end(), std::back_inserter(u8name));

                if (sit->second.name.empty())  // default data stream
                {
                    os << '\t'
                        << u8name << '\t'
                        << sit->second.realSize << std::endl;
                }
                else
                {
                    u8stream.clear();
                    utf8::utf16to8(sit->second.name.begin(), sit->second.name.end(), std::back_inserter(u8stream));
                    os << '\t'
                        << u8name << ':' << u8stream << '\t'
                        << sit->second.realSize << std::endl;
                }
            }
        }
    }

    std::string newPrefixDir;
    for (nit = it->second.begin(); nit != it->second.end(); ++nit)
    {
        if (nit->isdir)
        {
            newPrefixDir.assign(prefixDir);
            newPrefixDir.push_back('\\');
            utf8::utf16to8(nit->name.begin(), nit->name.end(), std::back_inserter(newPrefixDir));
            //os << newPrefixDir << std::endl;
            PrintInternal(newPrefixDir, os, nit->mftRef);
        }
    }
}
