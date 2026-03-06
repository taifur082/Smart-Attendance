#ifndef DTYPES_H_INCLUDED
#define DTYPES_H_INCLUDED

#include <string>
using namespace std;
struct ReaderInfo
{
  // the following constants are reader constants
  // they cannot be changed.
  int Serial = 0;
  char VersionInfo[2] = {0, 0};
  unsigned char Antenna = 0;

  // the following variables are reader parameters
  // which can be change. Call SetSettings function
  // to change the settings.
  unsigned char ComAdr;
  unsigned char ReaderType;
  unsigned char Protocol;
  unsigned char Band;
  unsigned char Power;
  unsigned char ScanTime;
  unsigned char BeepOn;
  unsigned char Reserved1;
  unsigned char Reserved2;

  int MaxFreq = 0;
  int MinFreq = 0;
  int BaudRate = 0;
};

struct ScanData
{
  unsigned char type;  // 1 byte
  unsigned char ant;   // 1 byte
  char RSSI;           // 1 byte
  unsigned char count; // 1 byte
  string data;         // 12 byte or 62 by7e
};

struct ScanResult
{
  unsigned char ant;     // 1 byte
  char RSSI;             // 1 byte
  unsigned char count;   // 1 byte
  unsigned char epclen;  // 1 byte
  unsigned char epc[12]; // 12 byte or 62 byte
};

struct Filter
{
  // these are word address (or even address)
  // unsigned char MaskMem;
  int MaskAdr;
  string MaskData;
};

enum MemoryType
{
  MEM_PASSWORD,
  MEM_EPC,
  MEM_TID,
  MEM_USER
};

enum TagType
{
  UNKNOWN = 0,

  HIGGS_3,
  HIGGS_4,
  HIGGS_EC,
  HIGGS_9,
  HIGGS_10,

  MONZA_4QT,
  MONZA_4E,
  MONZA_4D,
  MONZA_4I,
  MONZA_5,
  MONZA_R6,
  MONZA_R6P,
  MONZA_X2K,
  MONZA_X8K,
  IMPINJ_M730,
  IMPINJ_M750,
  IMPINJ_M770,
  IMPINJ_M775,

  UCODE_7,
  UCODE_8,
  UCODE_8M,
  UCODE_9,
  UCODE_DNA,
  UCODE_DNA_CITY,
  UCODE_DNA_TRACK,

  EM4423,

  KILOWAY_2005BR,
  KILOWAY_2005BL,
  KILOWAY_2005BT,
};

struct TagInfo
{
  int type;
  int tidlen;
  unsigned char tid[64];
  char chip[16];
  int epclen;
  int userlen;
  int pwdlen;
};

#endif // DTYPES_H_INCLUDED
