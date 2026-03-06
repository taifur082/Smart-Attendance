#ifndef DRIVER_903_H_INCLUDED
#define DRIVER_903_H_INCLUDED

#include "common.h"
#include "dtypes.h"

namespace mu903
{
  extern int baud_;
  extern bool transfer(unsigned char *command, int cmdsize, unsigned char *response, int responseSize, int sleepms, int pause);
  extern void setHandle(int h); // temporary workaround to shift the functions one by one

  extern bool GetSettings2(ReaderInfo *ri);
  extern bool SetSettings2(ReaderInfo *ri);

  extern "C" int Inventory2(bool filter);
  extern "C" int InventoryOne2();
  extern "C" bool GetResult2(unsigned char *scanresult, int index);
  extern "C" bool InventoryNB2(vector<ScanData> &v, bool filter);
  extern "C" bool InventoryB2(vector<ScanData> &v, bool filter);
  extern "C" bool InventoryTID2(vector<ScanData> &v);

  extern "C" bool GetTID2(unsigned char *epc, unsigned char epclen, unsigned char *tid, unsigned char *tidlen);
  extern "C" bool ReadMemWord2(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);
  extern "C" bool Read2(unsigned char *epc, unsigned epclen, unsigned char *data, int wnum, int windex, int memtype);
  extern "C" bool ReadWord2(unsigned char *epc, unsigned char epclen, unsigned char *data, int windex, int memtype);

  extern "C" bool Write2(unsigned char *epc, unsigned char epcLen, unsigned char *data, int Wsize, int windex, int memtype);
  extern "C" bool WriteWord2(unsigned char *epc, unsigned char epcLen, unsigned char *data, unsigned char windex, int memtype);
  extern "C" bool WriteMemWord2(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);
  extern "C" bool WriteEpc2(unsigned char *epc, unsigned char epclen, unsigned char *data);
  extern "C" bool WriteEpcWord2(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);

  extern bool SetFilter2(int maskAdrInByte, int maskLenInByte, unsigned char *maskDataByte);

  extern char GetGPI(unsigned char gpiNumber); // extern c is missing cz of namespace
  extern bool SetGPO(unsigned char gpiNumber);

  extern bool Auth2(unsigned char *password, unsigned char size);
  extern bool SetPassword2(unsigned char *epc, unsigned char epcLen, unsigned char *pass, unsigned char size);
  extern bool SetKillPassword2(unsigned char *epc, unsigned char epcLen, unsigned char *pass, unsigned char size);

  extern bool SetQ(unsigned char q);
  extern bool SetSession(unsigned char sess);

  extern void LastError(char *buffer);

}

#endif // DRIVER_903_H_INCLUDED
