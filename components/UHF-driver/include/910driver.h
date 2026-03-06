#ifndef DRIVER_910_H_INCLUDED
#define DRIVER_910_H_INCLUDED

#include "common.h"
#include "dtypes.h"

namespace mu910
{
  extern int baud_;

  extern bool transfer(unsigned char *command, int cmdsize, unsigned char *response, int responseSize, int sleepms, int pause);
  extern void setHandle(int h); // temporary workaround to shift the functions one by one

  extern "C" bool GetSettings1(ReaderInfo *ri);
  extern "C" bool SetSettings1(ReaderInfo *ri);

  extern "C" int Inventory1(bool filter);
  extern "C" int InventoryOne1();
  extern "C" bool GetResult1(unsigned char *scanresult, int index);
  extern "C" bool InventoryNB1(vector<ScanData> &v, bool filter);
  extern "C" bool InventoryB1(vector<ScanData> &v, bool filter);
  extern "C" bool InventoryTID1(vector<ScanData> &v);

  extern "C" bool GetTID1(unsigned char *epc, unsigned char epclen, unsigned char *tid, unsigned char *tidlen);
  extern "C" bool ReadMemWord1(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);
  extern "C" bool Read1(unsigned char *epc, unsigned epclen, unsigned char *data, int wnum, int windex, int memtype);
  extern "C" bool ReadWord1(unsigned char *epc, unsigned char epclen, unsigned char *data, int windex, int memtype);

  extern "C" bool Write1(unsigned char *epc, unsigned char epcLen, unsigned char *data, int Wsize, int windex, int memtype);
  extern "C" bool WriteWord1(unsigned char *epc, unsigned char epcLen, unsigned char *data, unsigned char windex, int memtype);
  extern "C" bool WriteMemWord1(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);
  extern "C" bool WriteEpc1(unsigned char *epc, unsigned char epclen, unsigned char *data);
  extern "C" bool WriteEpcWord1(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);

  extern bool SetFilter(int maskAdrInByte, int maskLenInByte, unsigned char *maskDataByte);

  extern bool Auth1(unsigned char *password, unsigned char size);
  extern bool SetPassword1(unsigned char *epc, unsigned char epcLen, unsigned char *pass, unsigned char size);
  extern bool SetKillPassword1(unsigned char *epc, unsigned char epcLen, unsigned char *pass, unsigned char size);

  extern bool SetQ(unsigned char q);
  extern bool SetSession(unsigned char sess);

  extern void LastError(char *buffer);
}

#endif // 910DRIVER_H_INCLUDED
