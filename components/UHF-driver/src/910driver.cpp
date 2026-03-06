#include <stdio.h>
#include "platform.h"

#include "910driver.h"
#include "common.h"

#include <iostream>
#include <iomanip> // For setw
#include <time.h>
#include <string>
#include <vector>

#define PRESET_VALUE 0xFFFF
#define POLYNOMIAL 0x8408

extern string error_;
namespace mu910 {

enum MaskMemoryTypes {
  RFU_BANK = 0x00,
  EPC_BANK = 0x10,
  TID_BANK = 0x20,
  USER_BANK = 0x30

};

int h_ = 0;

static unsigned char raw912Settings_[ 33 ];
static int scantime_ = 300;
int baud_;
vector<ScanData> scan_;
static Filter filter_; //MaskAdr*, MaskLen and MaskData*
static string password_ = string(4,0);

unsigned char session_ = 0x00;
unsigned char q_ = 2;
unsigned char readertype_ = 16;

static void emptyserial() {
  unsigned char buff[ 128 ];
  // empty the serial buffer
  platform_read(h_,buff,sizeof(buff),(unsigned short int*)buff,0);
}

void setHandle(int h) {
  h_ = h;
}

static void printScanDataVector(vector<ScanData>& scanDataVector) {
  std::cout << std::left
    << std::setw(8) << "Type"
    << std::setw(8) << "Ant"
    << std::setw(8) << "RSSI"
    << std::setw(8) << "Count"
    << "Data" << std::endl;

  for (int i = 0; i < scanDataVector.size(); i++) {
    std::cout << std::setw(8) << static_cast<int>(scanDataVector[ i ].type)
      << std::setw(8) << static_cast<int>(scanDataVector[ i ].ant)
      << std::setw(8) << static_cast<int>(scanDataVector[ i ].RSSI)
      << std::setw(8) << static_cast<int>(scanDataVector[ i ].count)
      << tohex(scanDataVector[ i ].data) << std::endl;
  }
}

unsigned short gencrc(unsigned char const* data,unsigned char n) {
  unsigned char i,j;
  unsigned short int crc = PRESET_VALUE;

  for (i = 0; i < n; i++) {
    crc = (crc ^ (data[ i ] & 0xFF)) & 0xFFFF;
    // printf("%04X ", crc);
    for (j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = ((crc >> 1) ^ POLYNOMIAL) & 0xFFFF;
      }
      else {
        crc = (crc >> 1) & 0xFFFF;
      }
    }
  }
  return crc;
}

bool verifycrc(unsigned char* data) {
  // int len = data[4]; // first byte contains the length
  // if the response is a chain like inventory then verify crc verifies only the first response
  int len = data[ 4 ];
  //    if (len == 0)
  //        return false;
  size_t rawSize = len + 5;
  unsigned char dataRaw[ rawSize ];
  memcpy(dataRaw,data,rawSize);

  unsigned int crc = gencrc(data,rawSize);
  unsigned char crcl = crc & 0xFF;
  unsigned char crch = (crc >> 8);
  if (Debug_) {
    printf("[verifyCRC]:>>\n");
    printarr(data,rawSize + 2);
    printf("FULL GEN CRC = %04X, Len = %d, CRCH = %02X, CRCL = %02X\n",crc,len,crch,crcl);
    printf("Recieved CRC %2X %2X, len = %d\n",data[ rawSize ],data[ rawSize + 1 ],data[ 4 ]);
    printf("<<[verifyCRC]\n");
  }
  if (data[ rawSize ] == crch && data[ rawSize + 1 ] == crcl) {
    return true;
  }
  return false;
}

bool transfer(unsigned char* command,int cmdsize,unsigned char* response,int responseSize,int sleepms,int pause) {
  unsigned short n;
  unsigned short crc = gencrc(command,cmdsize - 2);
  command[ cmdsize - 1 ] = crc & 0xFF;
  command[ cmdsize - 2 ] = (crc >> 8);

  if (Debug_) {
    printf("[transfer:send()]:>>\n");
    printarr(command,cmdsize);
    printf("<<[transfer:send()]\n");
  }
  // stopwatch(true);
  const int DEFAULT_RETRIES = 2;
  int retries = DEFAULT_RETRIES;
  bool fail = false;
  do {
    int e = platform_write(h_,command,cmdsize,&n);
    if (e == 0) {
      fail = true;
      if (pause > 0) {
        platform_sleep(pause);
      }
      continue;
    }
    fail = false;
    break;
  } while (retries--);
  if (fail) {
    printf("[ERROR] Send command failed.\n");
    return false;
  }

  memset(response,0,responseSize);
  if (pause > 0) {
    platform_sleep(pause);
  }
  retries = DEFAULT_RETRIES;
  do {
    int e = platform_read(h_,response,responseSize,&n,sleepms);
    if (e == 0) {
      fail = true;
      if (pause > 0) {
        platform_sleep(pause);
      }
      continue;
    }
    fail = false;
    break;
  } while (retries--);
  if (fail) {
    printf("[ERROR] Read command failed.\n");
    return false;
  }

  bool crcmatch = verifycrc(response);

  // printf("Read Finished in = %5.2f, size = %d\n", stopwatch(false), n);
  if (Debug_) {
    printf("[transfer:read()]:>>\n");
    printarr(response,n);
    if (crcmatch) {
      printf("CRC match.\n");
    }
    else {
      printf("CRC MISMATCH!\n");
    }
    printf("<<[transfer:read()]\n");
    // platform_flush(h_);
  }
  if (Debug_) {
    printf("TRANSFER: %d\n",crcmatch);
  }
  return crcmatch;

  /*
      //printarr(response, n);
      //printf("n = %d, e = %d\n", n, e);
      if (n > 0)
          return n;
      //printarr(response, response[0]+1);
      return 0;
  */
}

static int getBaudIndex10(int baud) {
  if (baud >= 15000 && baud < 30000)
    baud = 1;
  else if (baud >= 30000 && baud < 45000)
    baud = 2;
  else if (baud >= 45000 && baud < 80000)
    baud = 3;
  else if (baud >= 80000)
    baud = 4;
  else
    baud = 0;
  return baud;
}

static int getBaudIndex(int baud) {
  if (baud >= 15000 && baud < 30000)
    baud = 1;
  else if (baud >= 30000 && baud < 45000)
    baud = 2;
  else if (baud >= 45000 && baud < 80000)
    baud = 5;
  else if (baud >= 80000)
    baud = 6;
  else
    baud = 0;
  return baud;
}

static int getBaudrateValue(int baud) {
  // 9600, 19200, 38400, 57600, 115200
  switch (baud) {
  case 0:
    return 9600;
  case 1:
    return 19200;
  case 2:
    return 38400;
  case 3:
    return 57600; // for 910
  case 4:
    return 115200; // for 910
  case 5:
    return 57600;
  case 6:
    return 115200;
  default:
    return -1;
  }
}

static bool getSettings(ReaderInfo* ri) {
  // cf ff 00 72 00 17 a5
  unsigned char command[] = { 0xCF, 0xFF, 0x00, 0x72, 0, 0, 0 };
  unsigned char response[ 33 ];
  int retry = 2;
  bool ok;
#if defined(__APPLE__) || defined(ARM)
  int delay = 200;
#else
  int delay = 100;
#endif // __APPLE__
  do {
    emptyserial();
    ok = transfer(command,(int)sizeof(command),response,(int)sizeof(response),100,delay);
  } while (retry-- && !ok);
  if (!ok || response[ 5 ] != 0x00) {
    error_ = "Failed to get Settings Parameters..";
    return false;
  }

  memcpy(raw912Settings_,&response,sizeof(response));

  ((unsigned char*)&ri->Antenna)[ 0 ] = response[ 12 ];
  ri->Antenna = response[ 12 ];
  ri->ComAdr = response[ 6 ];
  ri->ReaderType = 0x0F;
  // protocol is below
  ri->Protocol = response[ 7 ];
  ri->Band = response[ 13 ];
  ri->Power = response[ 21 ] - 7;
  ri->ScanTime = scantime_ / 100; // response[28]; // volatile value
  ri->BaudRate = getBaudrateValue(response[ 10 ]);

  scantime_ = ri->ScanTime * 100;
  baud_ = ri->BaudRate;
  q_ = response[ 23 ];
  session_ = response[ 24 ];

  short startFreqI = 0x0000;
  startFreqI = (((startFreqI | response[ 14 ]) << 8) & 0xFFFF) | response[ 15 ];
  short startFreqD = 0x0000;
  startFreqD = (((startFreqD | response[ 16 ]) << 8) & 0xFFFF) | response[ 17 ];
  //Fmin = STRATFREI + STRATFRED/1000
  int minFreq = startFreqI + startFreqD / 1000.0f;
  short stepFreq = 0x0000;
  stepFreq = (((stepFreq | response[ 18 ]) << 8) & 0xFFFF) | response[ 19 ];
  unsigned char CN = response[ 20 ];
  //reader info is int so can't store floats in there. prev block has no use

//    printf("\nStartFreqI= %d\nStartFreqD= %d\nstepFreq= %d\nCN=%d", startFreqI, startFreqD, stepFreq, CN);

  if (response[ 29 ] != 0x00) {
    ri->BeepOn = 1;
  }
  else {
    ri->BeepOn = 0;
  }

  if (ri->Protocol == 0x00) {
    ri->Protocol = 0x6B;
  }
  else if (ri->Protocol == 0x01) {
    ri->Protocol = 0x77;
  }
  else {
    ri->Protocol = 0x6C;
  }

  //maximum frequency poin fmax = Fmin + STEPFRE*CN/1000 Mhz
  //set band and frequency
  switch (ri->Band) {
  case 1: // USA BAND
    ri->MinFreq = 902.75 * 1000.0;
    ri->MaxFreq = 927.25 * 1000.0;
    ri->Band = 'U';
    break;
  case 2: // KOREA BAND
    ri->MinFreq = 917.1 * 1000.0;
    ri->MaxFreq = 923.5 * 1000.0;
    ri->Band = 'K';
    break;
  case 3: // EU BAND
    ri->MinFreq = 865.1 * 1000.0;
    ri->MaxFreq = 868.1 * 1000.0;
    ri->Band = 'E';
    break;
  case 8: // CHINA2 BAND
    ri->MinFreq = 920.125 * 1000.0;
    ri->MaxFreq = 924.875 * 1000.0;
    ri->Band = 'C';
    break;
  }
  if (Debug_) {
    printsettings(ri);
  }

  return true;
}

bool GetSettings1(ReaderInfo* ri) {
  int retries = 10;
  int timeout = 0;
  bool ok;
  // 0xCF 0xFF 0x00 0x51 0x00 2Byte

  unsigned char command[] = { 0xCF, 0xFF, 0x00, 0x51, 0, 0, 0 };
  unsigned char response[ 84 ];
#if defined(__APPLE__)
  int delay = 1000;
#else
  int delay = 100;
#endif // __APPLE__
  int retry = 2;
  do {
    emptyserial();
    ok = transfer(command,sizeof(command),response,sizeof(response),50,delay);
    //        printarr(response, sizeof(response));
  } while (retry-- && !ok);

  if (response[ 5 ] != 0x00 || !ok) {
    error_ = "Couldn't Read Device Information!";
    printf("From getsettings fail;");
    fflush(stdout);
    return false;
  }

  ok = getSettings(ri); // get parameter - settings
  ri->Serial = ri->Serial + atoi(reinterpret_cast<const char*>(&response[ 74 ])) * 10000 + atoi(reinterpret_cast<const char*>(&response[ 78 ]));
  char* ver = ri->VersionInfo;
  ri->VersionInfo[ 0 ] = (unsigned char)atoi(reinterpret_cast<const char*>(&response[ 57 ]));
  ri->VersionInfo[ 1 ] = (unsigned char)atoi(reinterpret_cast<const char*>(&response[ 59 ]));
  ri->ReaderType = 0x17;
  printsettings(ri);
  return ok;
}

bool SetRegion(unsigned char band) {
  uint16_t startFreqI;
  uint16_t startFreqD;
  uint16_t stepFreq;
  unsigned char CN;
  unsigned char region;

  switch (band) {
  case 'U':
    startFreqI = 902;
    startFreqD = 750;
    stepFreq = 500;
    CN = 50;
    region = 0x01;
    break;
  case 'K':
    startFreqI = 917;
    startFreqD = 100;
    stepFreq = 200;
    CN = 32;
    region = 0x02;
    break;
  case 'E':
    startFreqI = 865;
    startFreqD = 100;
    stepFreq = 200;
    CN = 15;
    region = 0x03;
    break;
  case 'C':
    startFreqI = 920;
    startFreqD = 125;
    stepFreq = 250;
    CN = 20;
    region = 0x08;
    break;
  default:
    return false;
  }

  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x55);
  cmd.push_back(0x08);//lenght
  cmd.push_back(region);
  cmd.push_back((unsigned char)(startFreqI >> 8));
  cmd.push_back((unsigned char)startFreqI);
  cmd.push_back((unsigned char)(startFreqD >> 8));
  cmd.push_back((unsigned char)startFreqD);
  cmd.push_back((unsigned char)(stepFreq >> 8));
  cmd.push_back((unsigned char)stepFreq);
  cmd.push_back(CN);
  cmd.push_back(0x00);//crc
  cmd.push_back(0x00); //crc

  unsigned char* command = (unsigned char*)cmd.data();
  unsigned char cmdlen = cmd.size();
  printarr(command,15);
  unsigned char response[ 8 ];
  int retries = 2;
  bool ok;
  do {
    ok = transfer(command,cmdlen,response,sizeof(response),10,30);
  } while (retries-- && !ok);

  if (!ok) {
    error_ = "FAILED to send write request.";
    return false;
  }
  if (response[ 5 ] == 0x00) {
    return true;
  }
  else if (response[ 5 ] == 0x01) {
    error_ = "Parameter Error";
    if (Debug_) {
      printf("%s",error_.c_str());
    }
  }
  return false;
}

static bool SetRfPower(unsigned char powerDbm) {
  powerDbm = ((uint8_t)powerDbm) + 7;
  unsigned char command[] = { 0xCF, 0xFF, 0x00,0x53, 0x02, powerDbm, 0x00, 0, 0 };
  unsigned char response[ 8 ];
#if defined(__APPLE__) || defined(ARM)
  int delay = 60;
#else
  int delay = 20;
#endif // __APPLE__
  bool ok = transfer(command,sizeof(command),response,sizeof(response),20,delay);
  if (!ok) {
    error_ = "FAILED to send write request.";
    return false;
  }
  return true;
}

static bool SetAddress(unsigned char ComAdrData) {
  unsigned char command[] = { 0x05, 0x00, 0x2F, ComAdrData, 0, 0 };
  unsigned char response[ 6 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),100,10);
  if (!ok) {
    error_ = "FAILED to send write request.";
    return false;
  }
  return true;
}

static bool SetBeep(bool on) {
  int retries = 10;
  do {
    unsigned char command[] = { 0xCF, 0x00, 0x00, 0x5B, 0x02, 0x01, on, 0, 0 };
    unsigned char response[ 9 ];
    bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
    if (!ok)
      continue;
    return true;
  } while (retries--);
  return false;
}

//TO-DO
bool SetScanTime(unsigned char scantime) {
  //no reader scantime option so localizing
  scantime_ = scantime * 100;
  return true;
}

bool SetBaudRate(int baud) {
  baud = getBaudIndex10(baud);
  unsigned char command[] = { 0xCF, 0xFF, 0x00, 0x5A, 0x02, 0x01, (unsigned char)baud, 0, 0 };
  unsigned char response[ 9 ];
  int retries = 10;
  do {
    bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
    if (!ok)
      continue;
    break;
  } while (retries--);
  //    baud_ = baud;
  return true;
}

bool SetSettings1(ReaderInfo* ri) {
  ReaderInfo old;
  string cmd;

  bool status = GetSettings1(&old);
  if (!status) {
    error_ = "Failed to retrieve settings.\n";
    return false;
  }
  error_ = "";
  bool verify = true;
  if (old.Band != ri->Band) {
    // change band
    verify &= SetRegion(ri->Band);
    if (!verify)
      error_ += "Failed To Set Region. ";
  }
  if (old.BeepOn != ri->BeepOn) {
    // change beep setting
    verify &= SetBeep(ri->BeepOn);
    if (!verify)
      error_ += "Failed To Set Beep. ";
  }
  if (old.Power != ri->Power) {
    // change power
    verify &= SetRfPower(ri->Power);
    if (!verify)
      error_ = "Failed To Set Power. ";
  }
  if (old.ScanTime != ri->ScanTime) {
    // change scan time
    verify &= SetScanTime(ri->ScanTime);
    if (!verify)
      error_ = "Failed To Set ScanTime. ";
  }
  if (!verify) {
    if (Debug_) {
      printf("error: %s\n",error_.c_str());
    }
    return false;
  }
  // now verify settings
  ReaderInfo riv;
  status = GetSettings1(&riv);
  if (!status) {
    return false;
  }
  verify = true;
  if (Debug_) {
    printf("-------ri------\n");
    printsettings(ri);
    printf("-------riv------\n");
    printsettings(&riv);
    printf("-------------\n");
    printf("verify: %d\n",verify);
  }
  if (ri->Band != riv.Band) {
    error_ += "Failed to verify Band settings. ";
    verify = false;
  }
  if (ri->BeepOn != riv.BeepOn) {
    error_ += "Failed to verify Beep settings. ";
    verify = false;
  }
  if (ri->Power != riv.Power) { // +7 = power has some offset 26-33 conversion
    error_ += "Failed to verify Power settings. ";
    verify = false;
  }
  if (ri->ScanTime != riv.ScanTime) {
    error_ += "Failed to verify Scan Time settings. ";
    verify = false;
  }
  if (baud_ != ri->BaudRate) {
    SetBaudRate(ri->BaudRate);
  }
  return verify;
}

bool GetResult1(unsigned char* scanresult,int index) {
  if (index >= (int)scan_.size())
    return false;
  ScanData sd = scan_[ index ];
  int epcLength = sd.data.size();
  if (Debug_) {
    printf(">>GetResult:pointer_cast<>\n");
  }
  //    ScanResult *sr = (ScanResult*)scanresult;
  if (Debug_) {
    printf("<<GetResult:pointer_cast<>\n");
  }
  if (Debug_) {
    printf(">>ScanResult size = %d <<\n",sizeof(ScanResult));
  }
  // ignore the following
  //    sr->ant = sd.ant;
  //    sr->RSSI = sd.RSSI;
  //    sr->count = sd.count;
  //    sr->epclen = epcLength;
  /*
      struct ScanResult {
          unsigned char ant;     // 1 byte
          char RSSI;             // 1 byte
          unsigned char count;   // 1 byte
          unsigned char epclen;  // 1 byte
          unsigned char epc[12]; // 12 byte or 62 byte
      };
  */
  // We will manually put the values making sure that this transfer is platform independent
  if (sd.data.size() > epcLength)
    epcLength = 12;
  if (epcLength > 12)
    epcLength = 12;
  epcLength = 12;

  unsigned char* p = scanresult;
  *p++ = sd.ant;
  *p++ = sd.RSSI;
  *p++ = sd.count;
  *p++ = epcLength;
  memcpy(p,(char*)sd.data.data(),epcLength);
  if (Debug_) {
    string epcstr = string((char*)sd.data.data(),epcLength);
    printf("[RESULT] Ant: %d, RSSI: %d, Count: %d, EPC len: = %d, EPC: %s\n",sd.ant,sd.RSSI,sd.count,epcLength,tohex(epcstr).c_str());
  }
  // memcpy((char*)sr->epc,(char*)sd.data.data(),epcLength);
  return true;
}

static bool SetInventoryMask() {
  // RFM SET SEL PARAM - AKA filter
  uint8_t filterLen = filter_.MaskData.size();
  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x10); //  command SET sel parameter
  cmd.push_back(8 + filterLen);  // length
  cmd.push_back(0x00); // protocol 0x00= ISO
  cmd.push_back(0x00); // session
  cmd.push_back(0x00); // No-Truncated
  cmd.push_back(0x00); // Action(selects if matches)
  cmd.push_back(EPC_BANK); // Membank
  unsigned bitadr = filter_.MaskAdr * 8 + 32;
  cmd.push_back((unsigned char)(bitadr >> 8));   // MaskAdr
  cmd.push_back((unsigned char)(bitadr & 0xFF)); // MaskAdr
  cmd.push_back(filter_.MaskData.size() * 8);    // MaskLen
  cmd += filter_.MaskData;                       // MaskData
  cmd.push_back(0x00); // CRC
  cmd.push_back(0x00); // CRC

  unsigned char* command = (unsigned char*)cmd.data();
  printarr(command,cmd.size());
  //it transfer here and discards the response
  unsigned char response[ 9 ];
  int retry = 2;
  bool ok;
  emptyserial();
  do {
    ok = transfer(command,cmd.size(),response,sizeof(response),20,20);
  } while (retry-- && !ok);

  if (!ok || response[ 5 ] != 0x00) {
    error_ = "Couldn't set Mask!";
    return false;
  }

  // RFM SET QUERY PARAM
  cmd.clear();
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x12);  // SET query cmd
  cmd.push_back(0x04);   // length
  cmd.push_back(0x00);  // Protocol
  cmd.push_back(0x00);  // selection mode -02 matches with previously selected
  cmd.push_back(session_); // session
  cmd.push_back(0x00); // target
  cmd.push_back(0x00); // CRC reserved
  cmd.push_back(0x00); // CRC reserved

  unsigned char* command1 = (unsigned char*)cmd.data();
  printarr(command,cmd.size());
  unsigned char response1[ 9 ];
  retry = 2;
  ok = false;
  emptyserial();
  do {
    ok = transfer(command1,cmd.size(),response1,sizeof(response1),20,20);
  } while (retry-- && !ok);

  if (!ok || response1[ 5 ] != 0) {
    error_ = "Couldn't set Mask for filter!";
    return false;
  }

  return true;
}

static bool stopInventory() {
  // crc can be matched with while loop adn empty serial but not needed. gets the job done
  unsigned char command[] = { 0xCF, 0xFF, 0x00, 0x02, 0x00, 0, 0 };
  unsigned char response[ 128 ];
  //no need to match the crc so delay is low
  if (Debug_) {
    printf("Meant to mismatch crc and fail transfer!\n");
  }
  emptyserial();
  transfer(command,sizeof(command),response,sizeof(response),0,5);

  return true;
}

static bool setInventoryParameters(unsigned char acsAddress,unsigned char acsLen,unsigned char memtype) {
  //sets which type of inventory next we'll query. tid or epc
  ReaderInfo ri;
  //    getSettings(&ri);

  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x71);
  cmd.push_back(0x19);                // length
  cmd.push_back(raw912Settings_[ 6 ]);  // addr
  cmd.push_back(raw912Settings_[ 7 ]);  // rfidPro
  cmd.push_back(raw912Settings_[ 8 ]);  // work mode
  cmd.push_back(raw912Settings_[ 9 ]);  // Interface
  cmd.push_back(raw912Settings_[ 10 ]); // Baudrate*
  cmd.push_back(raw912Settings_[ 11 ]); // WgSet
  cmd.push_back(raw912Settings_[ 12 ]); // Ant
  for (int i = 13; i <= 20; i++) {
    cmd.push_back(raw912Settings_[ i ]); // Frequency parameters* 8 bit
  }
  cmd.push_back(raw912Settings_[ 21 ]); // Power*
  cmd.push_back(memtype);             // Inquiry Area
  cmd.push_back(raw912Settings_[ 23 ]); // Qvalue
  cmd.push_back(raw912Settings_[ 24 ]); // Session
  cmd.push_back(acsAddress);          // Acs address
  cmd.push_back(acsLen);              // Acs Data length
  cmd.push_back(raw912Settings_[ 27 ]); // FilterTime
  cmd.push_back(raw912Settings_[ 28 ]); // Trigger time* in s
  cmd.push_back(raw912Settings_[ 29 ]); // Buzzer time
  cmd.push_back(raw912Settings_[ 30 ]); // Polling Interval
  cmd.push_back(0x00);                // CRC Reserved
  cmd.push_back(0x00);                // CRC Reserverd

  unsigned char* command = (unsigned char*)cmd.data();
  unsigned char cmdlen = cmd.size();
  unsigned char response[ 8 ];
  int retry = 2;
  bool ok;
  do {
    emptyserial();
    ok = transfer(command,cmdlen,response,sizeof(response),100,100);
  } while (retry-- && !ok);

  if (!ok) {
    error_ = "Failed to Set Inventory Parameter";
    return false;
    printf("Set_settings issue coming from here \n");
  }

  return true;
}

/** \brief
 *  Gets Inventory, applies filter if specified.
 * \param port port name in the form of COM1, COM2 ... Baudrate must be one of the following:
 *     9600, 19200, 38400, 57600(default), 115200.
 * \return array which is multiple of dataLength. {type,ant,RSSI,count,epc[]}, where type, RSSI
 *     and count are int. epc array size is (dataLength - 4*4). The total array size will be even.
 *     If the operation fails, returns 0;
 */
int Inventory1(bool filter) {
  scan_.clear();
  bool success = InventoryNB1(scan_,filter);
  if (!success)
    return 0;

  return (unsigned)scan_.size();
}

/** \brief
 *  Gets Single Tag Inventory.
 * \return 1, if a single tag found, otherwise returns 0;
 */

int InventoryOne1() {

  // Clears scan vector
  // gives command - single inventory - interface signle cycle
  // gets a single tag verifies count
  // populates ScanData struct
  // returns int status

  scan_.clear();

  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // continue inventory command
  cmd.push_back(0x05); // length of data

  // DATA-invType and InvParam
  cmd.push_back(0x01); // type: scan by number of cycle
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // just Once

  cmd.push_back(0x00); // CRC placeholder
  cmd.push_back(0x00); // CRC placeholder

  unsigned char* command = (unsigned char*)cmd.data(); // into an array
  printarr(command,cmd.size());
  unsigned char cmdlen = cmd.size();

  unsigned char response[ 1024 ];

  stopwatch(true);
  if (Debug_) {
    printf("[single-inventory:scan()]>>\n");
  }
  // platform_sleep(100);
  if (Debug_) {
    printf("Scantime = %d\n",scantime_);
  }
  int retry = 2;
  bool ok;

  do {
    emptyserial();
    ok = transfer(command,cmdlen,response,sizeof(response),100,scantime_); // scantime_ + 10);
  } while (retry-- && !ok);
  stopInventory();
  if (Debug_) {
    printf("<<[inventory:scan()]\n");
  }
  if (!ok) {
    return false;
  }
  int offset = 5;
  int epclen = response[ offset + 5 ];
  unsigned char data[ epclen ];
  ScanData sd;
  short rssi = rssi & 0x0000;

  rssi = ((rssi | response[ offset + 1 ]) << 8) | response[ offset + 2 ];
  sd.RSSI = (char)(rssi / 10);
  sd.ant = response[ offset + 3 ];
  sd.count = 1;
  sd.type = MEM_EPC;
  memcpy(data,response + (offset + 6),epclen);
  sd.data = string((char*)data,epclen);
  scan_.push_back(sd);
  if (Debug_) {
    printScanDataVector(scan_);
  }

  return true;
}

static void appendSD(vector<ScanData>& v,ScanData sd) {
  // this function is to remove duplicate of epc
  int idx = 0;
  bool exists = false;

  for (int i = 0; i < v.size(); i++) {
    if (v[ i ].data == sd.data) {
      exists = true;
      idx = i;
      break;
    }
  }
  if (exists) {
    int8_t count = (int8_t)v[ idx ].count;
    count++;
    v[ idx ].count = (char)count;
  }
  else {
    v.push_back(sd);
  }
}

bool InventoryB1(vector<ScanData>& v,bool filter) {
  setInventoryParameters(0x00,0x00,MEM_EPC);
  bool ok;
  if (filter) {
    //call set mask with appropriate parameters
    ok = SetInventoryMask();
  }

  // cf ff 00 01 05 01 00 00 00 01 ef 78
  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // continue inventory command
  cmd.push_back(0x05); // length of data

  // DATA-invType and InvParam
  if (filter) {
    cmd.push_back(0x03); // type: mask
  }
  else {
    cmd.push_back(0x01); // type: scan by number of cycle
  }
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // just once

  cmd.push_back(0x00); // CRC placeholder
  cmd.push_back(0x00); // CRC placeholder

  unsigned char* command = (unsigned char*)cmd.data(); // into an array
  printarr(command,cmd.size());
  unsigned char cmdlen = cmd.size();

  unsigned char response[ 1024 ];
  if (Debug_) {
    printf("Scantime = %d\n",scantime_);
  }
  stopwatch(true);
  int retries = 10;
  do {
    emptyserial();
    ok = transfer(command,cmdlen,response,sizeof(response),200,1000);

  } while (retries-- && !ok);
  emptyserial();
  emptyserial();
  //    bool ok = transfer(command, cmdlen, response, sizeof(response), 10, scantime_ * 1.5);
  printarr(command,cmdlen);
  if (!ok)
    return false;

  int i = 0;
  while (i < sizeof(response)) {
    //        printf("LENGTH: %02x \n", response[i + 4]);
    if (response[ i + 4 ] != 0x12) {
      break;
    }

    int singleResponseLen = (int)response[ i + 10 ] + 13;
    unsigned char singleResponse[ singleResponseLen ];
    memcpy(singleResponse,response + i,singleResponseLen);

    if (Debug_) {
      printf("Response[%d]: ",(i / 25) + 1);
    }
    ScanData sd;
    sd.ant = singleResponse[ 8 ]; // 1 byte
    //    unsigned char type; // 1 byte

    //    string data; // 12 byte or 62 by7e
    unsigned char epclen = response[ i + 10 ]; // response[10]> length
    unsigned char data[ (int)epclen ];
    memcpy(data,singleResponse + 11,(int)epclen);
    sd.data = string((char*)data,epclen); // 12 byte or 62 by7e
    // implement a function to check if the epcExists or not;
    //  if exist then just edit that one increase the count
    sd.count = 0;
    short rssi = 0x0000;
    rssi = ((rssi | singleResponse[ 6 ]) << 8) | singleResponse[ 7 ];
    sd.RSSI = (char)(rssi / 10);
    sd.type = MEM_EPC;
    if (Debug_) {
      printarr(singleResponse,singleResponseLen);
      printf("\n");
    }

    if (singleResponse[ 10 ] == 0x3E) {
      // 62 bit

      i += (13 + 62);
    }
    else {
      // default 12 bit
      i += (13 + 12);
    }
    appendSD(v,sd);
  }
  if (Debug_) {
    printf("Inventory Finished in = %5.2fs\n",stopwatch(false));
  }
  if (Debug_) {
    printScanDataVector(scan_);
  }
  return true;
}

/**
 * Filter: x*, 2:x, x:2
 */
bool InventoryNB1(vector<ScanData>& v,bool filter) {
  setInventoryParameters(0x00,0x00,MEM_EPC); //this function is so thta inventory TID's parameter doesn't caues issue
  bool ok = false;
  if (filter) {
    //call set mask with appropriate parameters
    ok = SetInventoryMask();
  }

  if (!ok) {
    error_ = "Failed to set Filter!";
  }

  // cf ff 00 01 05 01 00 00 00 01 ef 78
  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // continue inventory command
  cmd.push_back(0x05); // length of data

  // DATA-invType and InvParam
  if (filter) {
    cmd.push_back(0x03); // type: mask
  }
  else {
    cmd.push_back(0x01); // type: scan by number of cycle
  }
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // just Once

  cmd.push_back(0x00); // CRC placeholder
  cmd.push_back(0x00); // CRC placeholder

  unsigned char* command = (unsigned char*)cmd.data(); // into an array
  unsigned char cmdlen = cmd.size();

  unsigned char response[ 2048 ];
  stopwatch(true);
  int retries = 3;
  do {
    emptyserial();
    ok = transfer(command,cmdlen,response,sizeof(response),20,scantime_ * 1.5);

  } while (retries-- && !ok);
  emptyserial();
  stopInventory();

  if (!ok)
    return false;

  int i = 0;
  while (i < sizeof(response)) {
    //        printf("LENGTH: %02x \n", response[i + 4]);
    //length 12+6 = 0x12 or 62+6 = 68 = 0x44
    if (response[ i + 4 ] != 0x12) {
      break;
    }

    int singleResponseLen = (int)response[ i + 10 ] + 13;
    unsigned char singleResponse[ singleResponseLen ];
    memcpy(singleResponse,response + i,singleResponseLen);

    if (Debug_) {
      printf("Response[%d]: ",(i / 25) + 1);

    }
    ScanData sd;
    sd.ant = singleResponse[ 8 ]; // 1 byte
    //    unsigned char type; // 1 byte

    //    string data; // 12 byte or 62 by7e
    unsigned char epclen = response[ i + 10 ]; // response[10]> length
    unsigned char data[ (int)epclen ];
    memcpy(data,singleResponse + 11,(int)epclen);
    sd.data = string((char*)data,epclen); // 12 byte or 62 by7e
    // implement a function to check if the epcExists or not;
    //  if exist then just edit that one increase the count
    sd.count = 0;
    short rssi = 0x0000;
    rssi = ((rssi | singleResponse[ 6 ]) << 8) | singleResponse[ 7 ];
    sd.RSSI = (char)(rssi / 10);
    sd.type = MEM_EPC;
    if (Debug_) {
      printarr(singleResponse,singleResponseLen);
      printf("\n");
    }

    if (singleResponse[ 10 ] == 0x3E) {
      // 62 bit

      i += (13 + 62);
    }
    else {
      // default 12 bit
      i += (13 + 12);
    }
    appendSD(v,sd);
  }
  if (Debug_) {
    printf("Inventory Finished in = %5.2fs\n",stopwatch(false));
  }
  if (Debug_) {
    printScanDataVector(scan_);
  }
  return true;
}

bool InventoryTID1(vector<ScanData>& v) {
  // First call set param function and set the memtype
  setInventoryParameters(0x00,0x00,MEM_TID);

  // query for inventory and rest is same like inventory
  // cf ff 00 01 05 01 00 00 00 01 ef 78
  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // continue inventory command
  cmd.push_back(0x05); // length of data

  // DATA-invType and InvParam
  cmd.push_back(0x01); // type: scan by number of cycle
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // just Once

  cmd.push_back(0x00); // CRC placeholder
  cmd.push_back(0x00); // CRC placeholder

  unsigned char* command = (unsigned char*)cmd.data(); // into an array
  printarr(command,cmd.size());
  unsigned char cmdlen = cmd.size();

  unsigned char response[ 1024 ];
  printf("Scantime = %d\n",scantime_);
  stopwatch(true);
  int retries = 10;
  bool ok;
  do {
    ok = transfer(command,cmdlen,response,sizeof(response),200,1000);

  } while (retries-- && !ok);
  emptyserial();
  emptyserial();
  //    bool ok = transfer(command, cmdlen, response, sizeof(response), 10, scantime_ * 1.5);
  printarr(command,cmdlen);
  if (!ok)
    return false;

  int i = 0;
  while (i < sizeof(response)) {
    //        printf("LENGTH: %02x \n", response[i + 4]);
    if (response[ i + 4 ] != 0x12) {
      break;
    }

    int singleResponseLen = (int)response[ i + 10 ] + 13;
    unsigned char singleResponse[ singleResponseLen ];
    memcpy(singleResponse,response + i,singleResponseLen);

    if (Debug_) {
      printf("Response[%d]: ",(i / 25) + 1);

    }
    ScanData sd;
    sd.ant = singleResponse[ 8 ]; // 1 byte
    //    unsigned char type; // 1 byte

    //    string data; // 12 byte or 62 by7e
    unsigned char epclen = response[ i + 10 ]; // response[10]> length
    unsigned char data[ (int)epclen ];
    memcpy(data,singleResponse + 11,(int)epclen);
    sd.data = string((char*)data,epclen); // 12 byte or 62 by7e
    // implement a function to check if the epcExists or not;
    //  if exist then just edit that one increase the count
    sd.count = 0;
    short rssi = 0x0000;
    rssi = ((rssi | singleResponse[ 6 ]) << 8) | singleResponse[ 7 ]; //signed
    sd.RSSI = (char)(rssi / 10);
    sd.type = MEM_EPC;

    if (Debug_) {
      printarr(singleResponse,singleResponseLen);
      printf("\n");
    }

    if (singleResponse[ 10 ] == 0x3E) {
      // 62 bit

      i += (13 + 62);
    }
    else {
      // default 12 bit
      i += (13 + 12);
    }
    appendSD(v,sd);
  }
  if (Debug_) {
    printf("Inventory Finished in = %5.2fs\n",stopwatch(false));
  }
  if (Debug_) {
    printScanDataVector(scan_);
  }

  // reset the set param
  setInventoryParameters(0x00,0x00,MEM_EPC);
  return true;
}

bool Auth1(unsigned char* password,unsigned char size) {
  if (password) {
    if (size < 4) {
      error_ = "Password length must be at lease 8 bytes.";
      return false;
    }
    password_ = string((char*)password,4);
    return true;
  }
  password_ = string(4,0);
  return true;
}

bool SetPassword1(unsigned char* epc,unsigned char epcLen,unsigned char* pass,unsigned char size) {
  if (size != 4) {
    error_ = "Password must be 4 bytes long.";
    return false;
  }

  //    return WriteWord((unsigned char*)epc,epcLen,pass,3,MEM_PASSWORD);
  return Write((unsigned char*)epc,epcLen,pass,size / 2,2,MEM_PASSWORD);
}

bool SetKillPassword1(unsigned char* epc,unsigned char epcLen,unsigned char* pass,unsigned char size) {
  return Write((unsigned char*)epc,epcLen,pass,size / 2,0,MEM_PASSWORD);
}

//bool GetKillPassword(unsigned char *epc, int epclen, unsigned char *pass, unsigned char *passlen) {
//    *passlen = 8;
//    return Read((unsigned char *)epc, epclen, pass, 4, 0, MEM_PASSWORD);
//}

static bool SelectMask(unsigned char* epc,unsigned char epclen) {
  string cmd;
  cmd.push_back(0xCF);
  cmd.push_back(0xFF);
  cmd.push_back(0x00);
  cmd.push_back(0x07); //SETISO_SELECT_MASK
  cmd.push_back(0x0F); //length: 15 for not but dynamic masking will require it to be dynamic
  cmd.push_back(0x00);
  cmd.push_back(0x00); //pointer = starting word point
  cmd.push_back(epclen);
  for (int i = 0; i < epclen; i++) {
    cmd.push_back(epc[ i ]);
  }
  cmd.push_back(0);//CRC Placeholder
  cmd.push_back(0);

  unsigned char* command = (unsigned char*)cmd.data();
  unsigned char cmdlen = cmd.size();
  unsigned char response[ 8 ];
#if defined(__APPLE__) || defined(ARM)
  int delay = 40;
#else
  int delay = 20;
#endif // __APPLE__
  int retries = 2;
  bool ok;
  do {
    emptyserial();
    ok = transfer(command,cmdlen,response,sizeof(response),20,delay);

  } while (retries-- && !ok);

  if (response[ 5 ] == 0x00 && ok) {
    return true;
  }
  else {
    error_ = "FAILED to select the specific tag to read the data. ";
  }

  return false;
}

bool GetTID1(unsigned char* epc,unsigned char epclen,unsigned char* tid,unsigned char* tidlen) {
  *tidlen = epclen;
  error_ = "";
  bool success = Read1(epc,epclen,tid,epclen / 2,0,MEM_TID);
  if (!success) {
    error_ += " Failed to getTID. ";
  }
    if (Debug_)
    {
      printf("TID =");
      printarr((unsigned char *)tid, 12);
    }
  return success;
}

bool Read1(unsigned char* epc,unsigned epclen,unsigned char* data,int wnum,int windex,int memtype) {
  // cout << "Password: " << tohex(password_) << endl;

  // get the epc - received
  // get the memory type - received
  // send the prepare command for that epc - done

  // send read command
  // extract the exact data - undertain about the length
  // set the data into tid or the type of memory that has been accessed

  if (data == 0) {
    error_ = "No writable data provided. ";
    return false;
  }
  if (epclen > 15) {
    char str[ 100 ];
    snprintf(str,sizeof(str),"EPC length is too long : %d. ",epclen);
    error_ = str;
    return false;
  }
  if (password_.size() < 4) {
    error_ = "Password is not set or does not match. ";
    return false;
  }

  SelectMask(epc,epclen);

  char* password = (char*)password_.data();
  string command;
  //    0x00: Reserved; 0x01: EPC; 0x02: TID; 0x03: User;
  if (memtype == MEM_EPC) {
    windex += 2;
  }

  command.push_back(0xCF);
  command.push_back(0xFF);
  command.push_back(0x00);
  command.push_back(0x03); //READIDO_TAG
  command.push_back(0x09); // length
  command.push_back(0x00); //option-unused
  for (int i = 0; i < 4; i++) {
    command.push_back(password[ i ]);
  }
  command.push_back(memtype);
  command.push_back((unsigned char)(windex >> 16));//word pointer
  command.push_back((unsigned char)(windex));//word pointer
  command.push_back(wnum);                    //word count
  command.push_back(0); // crc place holder
  command.push_back(0); // crc place holder

  //FUTURE USE A SET FILTER for duplicates

  unsigned char response[ 13 + (int)epclen + 1 + (wnum * 2) + 2 ]; // 12static + epc + wordcount+ data+ crc
  //    unsigned char response[200];// similar to inventory any number may be returned

  int retries = 3;
  bool ok;
  //    #if defined(__APPLE__) || defined(ARM)
#ifdef LINUX
  int delay = 250;
#else
  int delay = 20;
#endif // __APPLE__

  do {
    emptyserial();
    ok = transfer((unsigned char*)command.data(),command.size(),response,sizeof(response),50,delay);
    if (Debug_) {
      printarr(response,sizeof(response));
      printf("\nStatus of Read: %d\n",ok);
    }

  } while (retries-- && !ok);

  //set error
  if (!ok) {
    return false;
  }
  stopInventory(); //commnad

  //    printf("Response: ");
  //    printarr(response, sizeof(response));

  int dataLength = response[ 12 + (int)epclen + 1 ] * 2; //wordcount*2
  memcpy(data,response + (12 + epclen + 2),dataLength);
  if (Debug_) {
    printf("Received Data: ");
    printarr(data,dataLength);

  }

  //    printf("Datalength: %d\n", dataLength);
  //    printf("This is from READ DATA:\n");
  //    printarr(response, sizeof(response));

  return true;
}

bool ReadWord1(unsigned char* epc,unsigned char epclen,unsigned char* data,int windex,int memtype) {
  return Read(epc,epclen,data,1,windex,memtype); //only 1 word
}

bool ReadMemWord1(unsigned char* epc,unsigned char epclen,unsigned char* data,unsigned char windex) {
  return ReadWord1(epc,epclen,data,windex,MEM_USER);
}

bool Write1(unsigned char* epc,unsigned char epcLen,unsigned char* data,int wsize,int windex,int memtype) {
  // cout << "Password: " << tohex(password_) << endl;
  if (data == 0) {
    error_ = "No writable data provided.";
    return false;
  }
  if (epcLen > 15) {
    char str[ 100 ];
    snprintf(str,sizeof(str),"EPC length is too long : %d.",epcLen);
    error_ = str;
    return false;
  }
  if (password_.size() < 4) {
    error_ = "Password is not set or does not match.";
    return false;
  }
  SelectMask(epc,epcLen); //filter
  emptyserial();

  if (memtype == MEM_EPC) {
    windex += 2;                          // offset is 2 for EPC memory
  }
  else {
    windex = windex;          // no offset for other than EPC memory
  }
  char* password = (char*)password_.data();
  string cmd;
  cmd.push_back(0xCF);               // HEAD
  cmd.push_back(0xFF);               // address
  cmd.push_back(0x00);               // command
  cmd.push_back(0x04);               // command
  cmd.push_back(9 + (wsize * 2));        // length of payload
  cmd.push_back(0x00);               // option 0x00
  for (int i = 0; i < 4; i++) {
    cmd.push_back(password[ i ]);
  }
  cmd.push_back(memtype);             // memory type to be written
  cmd.push_back((unsigned char)(windex >> 16));    //word pointer
  cmd.push_back((unsigned char)(windex));        //word pointer
  cmd.push_back(wsize);
  cmd += string((char*)data,wsize * 2);         // data to be written

  cmd.push_back(0);                     // crc place holder
  cmd.push_back(0);                     // crc place holder
  unsigned char* command = (unsigned char*)cmd.data();

  // cout << "Command = " << tohex(command) << endl;

  unsigned char response[ 27 ];
  int retries = 2;
  bool ok;
  do {
    ok = transfer(command,cmd.size(),response,sizeof(response),150,80);
    // n = transfer((unsigned char*)command.data(), command.size(), response, sizeof(response), delay);
    if (!ok) {
      error_ = "FAILED to send write request.";
    }

    if (response[ 5 ] != 0) {
      ok = false;
    }

  } while (retries-- && !ok);
  if (response[ 5 ] == 0x00) {
    //write successful
    return true;
  }
  if (!ok) {
    error_ = "FAILED to write.";
  }

  return false;
}

bool WriteWord1(unsigned char* epc,unsigned char epcLen,unsigned char* data,unsigned char windex,int memtype) {
  //    do {
  bool success = Write(epc,epcLen,data,1,windex,memtype);
  if (memtype == MEM_EPC)
    return success;
  if (success) {
    unsigned char data2[ 2 ] = { 0, 0 };
    success = ReadWord(epc,epcLen,data2,windex,memtype);
    if (success) {
      if (data[ 0 ] == data2[ 0 ] && data[ 1 ] == data2[ 1 ])
        return true; // data successfully written.
    }
  }
  //    } while(retries--);
  return false;
}

bool WriteMemWord1(unsigned char* epc,unsigned char epclen,unsigned char* data,unsigned char windex) {
  bool b = Write(epc,epclen,data,1,windex,MEM_USER);
  if (b) {
    unsigned char data2[ 2 ] = { 0, 0 };
    // printarr(epc,epcLen);
    // printarr(epc2,epcLen);
    bool b = ReadWord(epc,epclen,data2,windex,MEM_USER);
    // printarr(data,2);
    // printarr((unsigned char*)data2,2);
    if (b) {
      if (data[ 0 ] == data2[ 0 ] && data[ 1 ] == data2[ 1 ]) {
        return true;
      }
    }
  }

  return b;
}
bool WriteEpcWord1(unsigned char* epc,unsigned char epclen,unsigned char* data,unsigned char windex) {
  bool b = WriteWord(epc,epclen,data,windex,MEM_EPC);
  if (!b) {
    unsigned char epc2[ 32 ];
    memcpy(epc2,epc,epclen);
    epc2[ windex * 2 ] = data[ 0 ];
    epc2[ windex * 2 + 1 ] = data[ 1 ];
    unsigned char data2[ 2 ] = { 0, 0 };
    // printarr(epc,epcLen);
    // printarr(epc2,epcLen);
    bool b = ReadEpcWord(epc2,epclen,data2,windex,MEM_EPC);
    // printarr(data,2);
    // printarr((unsigned char*)data2,2);
    if (b) {
      if (data[ 0 ] == data2[ 0 ] && data[ 1 ] == data2[ 1 ]) {
        return true;
      }
    }
  }
  return b;
}

bool WriteEpc1(unsigned char* epc,unsigned char epcLen,unsigned char* data) {
  bool ok = Write(epc,epcLen,data,epcLen / 2,0,MEM_EPC);
  if (ok) {
    unsigned char newepc[ epcLen ];
    ok = Read(data,epcLen,newepc,epcLen / 2,0,MEM_EPC);
    if (memcmp(data,newepc,epcLen) == 0) {
      //            printf("EPC verified.\n");
      return true;
    }
  }
  return false;
}

bool SetFilter(int maskAdrInByte,int maskLenInByte,unsigned char* maskDataByte) {
  if (maskAdrInByte < 0 || maskAdrInByte > 12) {
    error_ = "Mask starts at an illegal offset.";
    return false;
  }
  filter_.MaskAdr = maskAdrInByte;
  if (maskLenInByte > (12 - maskAdrInByte)) {
    error_ = "Mask length is greater than EPC length.";
    return false;
  }
  filter_.MaskData = string((char*)maskDataByte,maskLenInByte);
  string hex = tohex(filter_.MaskData);
  //    printf("MASK: MaskAdr = %d, MaskLen = %d, MaskData = %s\n",
  //           maskAdrInByte,
  //           filter_.MaskData.size(),
  //           hex.c_str());
  return true;
}

void LastError(char* buffer) {
  strcpy(buffer,error_.c_str());
}

bool SetQ(unsigned char q) {
  extern unsigned char q_;
  if (q_ > 15)
    return 1;
  q_ = q;
  return 0;
}

bool SetSession(unsigned char sess) {
  extern unsigned char session_;
  if (session_ > 3)
    return 1;
  session_ = sess;
  return 0;
}

}
