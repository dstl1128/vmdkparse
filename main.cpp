//
// Main
// Opens a VMDK, then try to parse the 1st partition as NTFS,
// construct the tree, prints out the entire hierarchy and,
// try to extract out the data of \WINDOWS\system32\notepad.exe
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#include "vmdk.h"
#include "ntfs.h"
#include "ntfs_file.h"
#include "ntfs_tree.h"

#include <stdexcept>
#include <stdlib.h>
#include <sstream>

struct Pause
{
    Pause() {}
    ~Pause()
    {
        std::cerr << "\nPress ENTER to end.\n";
        getchar();
    }
};

char const CMD_USAGE[] = "usage: %s vmdkfile {--dump partition# [internal file path] [output file]} | {--snapshot [output file]}\n";


int main(int argc, char * argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, CMD_USAGE, argv[0]);
        return 1;
    }

#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_) || defined(__DEBUG) || defined(__DEBUG__)
    Pause pause;
#endif

    try
    {
        disk::Vmdk vmdisk(argv[1]);
        vmdisk.Test();

        if (strcmp(argv[2], "--snapshot") == 0)
        {
            std::ofstream ofs;
            if (argc >= 4)
            {
                ofs.open(argv[3]);
                if (!ofs.is_open())
                    throw std::runtime_error("Can't open output file.");
            }

            // dump MBR
            if (argc >= 4)
            {
                std::ostringstream ostr;
                ostr << argv[3] << ".mbr" << ".bin";

                std::string outname(ostr.str());
                std::ofstream mbrOut(outname.c_str(), std::ios::binary);
                if (!mbrOut.is_open())
                    throw std::runtime_error("Can't open MBR output file.");

                char buf[512];
                vmdisk.RawSector(0, buf);
                mbrOut.write(&buf[0], sizeof(buf));
            }

            // dump all NTFS partitions'
            char part = 0;
            char drive[5] = { 'C', ':', 0 };
            disk::Partitions::iterator it = vmdisk.BeginPartition();
            for (; it != vmdisk.EndPartition(); ++it, ++part)
            {
                // dumps boot sector
                if (argc >= 4)
                {
                    std::ostringstream ostr;
                    ostr << argv[3] << ".boot" << (int)part << ".bin";

                    std::string outname(ostr.str());
                    std::ofstream bootOut(outname.c_str(), std::ios::binary);
                    if (!bootOut.is_open())
                        throw std::runtime_error("Can't open boot output file.");

                    char buf[512];
                    vmdisk.ReadSector(0, buf, part);
                    bootOut.write(&buf[0], sizeof(buf));
                }

                // dumps files/folders listing
                if (it->type == 0x7) // is NTFS
                {
                    ntfs::Ntfs ntfsdisk(vmdisk, part);
                    ntfsdisk.Test();
                    ntfs::Tree tree(ntfsdisk);

                    drive[0] = 'C' + part;
                    if (drive[0] < 'C' || drive[0] > 'Z')
                        throw std::runtime_error("Drive letter not enough.");

                    if (argc >= 4)
                        tree.Print(drive, ofs);
                    else
                        tree.Print(drive);
                }
            }
        }
        else if (strcmp(argv[2], "--dump") == 0)
        {
            // extract a single file
            int part = 0;
            if (argc >= 4)
                part = atoi(argv[3]);
            ntfs::Ntfs ntfsdisk(vmdisk, part);
            ntfsdisk.Test();
            ntfs::Tree tree(ntfsdisk);
            ntfs::File file(tree);

            file.Open((argc >= 5) ? argv[4] : "/WINDOWS/system32/notepad.exe");

            char buf[512];
            u64 size = file.Size();
            unsigned long reads;
            std::ofstream ofs(((argc >= 6) ? argv[5] : "dump.bin"), std::ios_base::binary);
            while (!file.Eof())
            {
                reads = file.Read(buf, (unsigned long)std::min<u64>(sizeof(buf), size));
                ofs.write(buf, reads);
            }
        }
        else
        {
            fprintf(stderr, CMD_USAGE, argv[0]);
            return 1;
        }
        return 0;
    }
    catch(char const * msg)
    {
        std::cerr << msg << std::endl;
        return 2;
    }
    catch(std::runtime_error & err)
    {
        std::cerr << err.what() << std::endl;
        return 3;
    }
    catch(std::exception & err)
    {
        std::cerr << err.what() << std::endl;
        return 7;
    }
}
