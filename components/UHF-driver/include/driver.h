#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED

#include "platform.h"

#include <vector>
#include <string>

#include <cstdint>
#include <time.h>
#include "910driver.h"
#include "common.h"
#include "dtypes.h"

using namespace std;

#define LIB_VERSION 0x0204

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif

extern bool Debug_;
// extern bool           SetInventoryMask();

/** \brief
 *  Opens a COM port.
 * \param port port name in the form of COM1, COM2 ... Baudrate must be one of the following:
 *     9600, 19200, 38400, 57600(default), 115200.
 * \return true if successful..
 *
 */
extern "C" DLLEXPORT bool OpenComPort(const char *port, int baud);
extern "C" DLLEXPORT void CloseComPort(void);
extern "C" DLLEXPORT bool SetBaudRate(int baud);
extern "C" DLLEXPORT int AvailablePorts(char *ports);
extern "C" DLLEXPORT bool LoadSettings(unsigned char *buff);
extern "C" DLLEXPORT bool SaveSettings(unsigned char *buff);

bool GetSettings(ReaderInfo *ri);
bool SetSettings(ReaderInfo *ri);

extern "C" DLLEXPORT int Inventory(bool filter);
extern "C" DLLEXPORT int InventoryOne();
extern "C" DLLEXPORT bool GetResult(unsigned char *scanresult, int index);
extern "C" DLLEXPORT bool InventoryNB(vector<ScanData> &v, bool filter);
extern "C" DLLEXPORT bool InventoryB(vector<ScanData> &v, bool filter);
extern "C" DLLEXPORT bool InventoryTID(vector<ScanData> &v);
extern "C" DLLEXPORT bool Set496Bits(bool bits496);
extern "C" DLLEXPORT bool Is496Bits();
extern "C" DLLEXPORT bool GetTID(unsigned char *epc, unsigned char epclen, unsigned char *tid, unsigned char *tidlen);

extern "C" DLLEXPORT bool Write(unsigned char *epc, unsigned char epcLen, unsigned char *data, int Wsize, int windex, int memtype);
extern "C" DLLEXPORT bool WriteWord(unsigned char *epc, unsigned char epcLen, unsigned char *data, unsigned char windex, int memtype);
extern "C" DLLEXPORT bool WriteMemWord(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);
extern "C" DLLEXPORT bool WriteEpc(unsigned char *epc, unsigned char epclen, unsigned char *data);
extern "C" DLLEXPORT bool WriteEpcWord(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);

extern "C" DLLEXPORT bool ReadMemWord(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex);
extern "C" DLLEXPORT bool Read(unsigned char *epc, unsigned epclen, unsigned char *data, int wnum, int windex, int memtype);
extern "C" DLLEXPORT bool ReadWord(unsigned char *epc, unsigned char epclen, unsigned char *data, int windex, int memtype);

extern "C" DLLEXPORT bool Auth(unsigned char *password, unsigned char size);
extern "C" DLLEXPORT bool SetPassword(unsigned char *epc, unsigned char epcLen, unsigned char *pass, unsigned char size);
extern "C" DLLEXPORT bool SetKillPassword(unsigned char *epc, unsigned char epcLen, unsigned char *pass, unsigned char size);

extern "C" DLLEXPORT void LastError(char *buffer);

extern "C" DLLEXPORT bool SetFilter(int maskAdrInByte, int maskLenInByte, unsigned char *maskDataByte);
extern "C" DLLEXPORT bool CreateFilter(char *filterDesc);

extern "C" DLLEXPORT bool TagExists(unsigned char *epc, unsigned char epclen);
extern "C" DLLEXPORT bool ReadEpcWord(unsigned char *epc, unsigned char epclen, unsigned char *data, int windex, int memtype);

extern "C" DLLEXPORT int GetTagInfo(unsigned char *tid, TagInfo *info);

extern "C" DLLEXPORT char GetGPI(unsigned char gpiNumber);
extern "C" DLLEXPORT bool SetGPO(unsigned char gpiNumber);

extern "C" DLLEXPORT void LibVersion(unsigned char *version);
extern "C" DLLEXPORT bool SetQ(unsigned char q);
extern "C" DLLEXPORT bool SetSession(unsigned char sess);

////TEMPORARY
// extern "C" DLLEXPORT bool SetScanTime(unsigned char scantime);
////================

extern "C" DLLEXPORT int KillTag_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Enum, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *errorcode, int FrmHandle);

/// ---- TODO ----
extern "C" DLLEXPORT int SetSaveLen(unsigned char *ComAdr, unsigned char SaveLen, int FrmHandle);
extern "C" DLLEXPORT int GetSaveLen(unsigned char *ComAdr, unsigned char *SaveLen, int FrmHandle);
extern "C" DLLEXPORT int GetBufferCnt_G2(unsigned char *ComAdr, int *Count, int FrmHandle);
extern "C" DLLEXPORT int SetTagCustomFunction(unsigned char *address, unsigned char *InlayType, int FrmHandle);

extern "C" DLLEXPORT int BlockErase_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Enum, unsigned char Mem, unsigned char WordPtr, unsigned char Num, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int Lock_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Enum, unsigned char select, unsigned char setprotect, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int SetPrivacyByEPC_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Enum, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int SetPrivacyWithoutEPC_G2(unsigned char *ComAdr, unsigned char *Password, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int ResetPrivacy_G2(unsigned char *ComAdr, unsigned char *Password, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int CheckPrivacy_G2(unsigned char *ComAdr, unsigned char *readpro, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int EASConfigure_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Enum, unsigned char *Password, unsigned char EAS, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int EASAlarm_G2(unsigned char *ComAdr, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int BlockWrite_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Wnum,
                                       unsigned char Enum, unsigned char Mem, unsigned char WordPtr, unsigned char *Writedata, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int ExtReadData_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Enum, unsigned char Mem, unsigned char *WordPtr, unsigned char Num, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, unsigned char *Data, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int ExtWriteData_G2(unsigned char *ComAdr, unsigned char *epc, unsigned char Wnum,
                                         unsigned char Enum, unsigned char Mem, unsigned char *WordPtr, unsigned char *Writedata, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *errorcode, int FrmHandle);

extern "C" DLLEXPORT int GetMonza4QTWorkParamter_G2(unsigned char *address, unsigned char *epc, unsigned char ENum, unsigned char *Password, unsigned char MaskMem, unsigned char *MaskAdr,
                                                    unsigned char MaskLen, unsigned char *MaskData, unsigned char *QTcontrol, int *Errorcode, int FrmHandle);

extern "C" DLLEXPORT int SetMonza4QTWorkParamter_G2(unsigned char *address, unsigned char *epc, unsigned char ENum, unsigned char QTcontrol, unsigned char *Password, unsigned char MaskMem,
                                                    unsigned char *MaskAdr, unsigned char MaskLen, unsigned char *MaskData, int *Errorcode, int FrmHandle);

#endif // DRIVER_H_INCLUDED
