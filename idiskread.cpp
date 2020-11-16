#include "idiskread.h"

disk::Partition::Partition()
: type(0), status(0), head(0), sector(0), cylinder(0), firstSectorLBA(0), numberBlock(0)
{
}

disk::Partition::Partition(disk::MbrPartition const & mbrpart)
: type(mbrpart.type),
  status(mbrpart.status),
  head(mbrpart.head),
  sector(mbrpart.sector & 0x3f),
  cylinder(mbrpart.cylinder | ((mbrpart.sector & 0xC0) << 2)),
  firstSectorLBA(mbrpart.firstSectorLBA),
  numberBlock(mbrpart.numberBlock)
{
}
