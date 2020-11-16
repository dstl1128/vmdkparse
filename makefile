CC = g++
CFLAGS = -Wall -Wextra -W -Wno-format -g -fpack-struct=8
OBJECTS = main.o file64.o ntfs_attr.o ntfs_datarun.o ntfs.o ntfs_file.o \
		  ntfs_index.o ntfs_layout.o ntfs_tree.o vmdk.o types.o idiskread.o \
		  ntfs_compress.o
EXE = vmdkparse

.SUFFIXES: .cpp .o

.cpp.o:
	$(CC) $(CFLAGS) $(OPTIONS) -c $<

.o:
	$(CC) $(CFLAGS) $^ -o $@

$(EXE): $(OBJECTS)
	$(CC) -o $@ $^
	chmod 775 $@

clean:
	rm $(EXE) $(OBJECTS)

install: $(EXE)

strip: $(EXE)
	strip $^
