#include <stdio.h>
#include "903driver.h"

#include "platform.h"
#include "common.h"

#include <string>
#include <vector>

#define PRESET_VALUE 0xFFFF
#define POLYNOMIAL 0x8408

extern string error_;

namespace mu903 {

int h_ = 0;

static int scantime_ = 300;
vector<ScanData> scan_;
int baud_;
static Filter filter_; //MaskAdr*, MaskLen and MaskData*
static string password_ = string(4,0);

unsigned char session_ = 0x00;
unsigned char q_ = 2;
unsigned char readertype_ = 15;

static void emptyserial() {
  unsigned char buff[ 128 ];
  // empty the serial buffer
  platform_read(h_,buff,sizeof(buff),(unsigned short int*)buff,1);
}

void setHandle(int h) {
  h_ = h;
}

unsigned short gencrc(unsigned char const* data,unsigned char n) {
  unsigned char i,j;
  unsigned short int  crc = PRESET_VALUE;

  for (i = 0; i < n; i++) {
    crc = (crc ^ (data[ i ] & 0xFF)) & 0xFFFF;
    //printf("%04X ", crc);
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
  int len = data[ 0 ]; // first byte contains the length
  //    if (len == 0)
  //        return false;
  unsigned short crc = gencrc(data,len - 1);
  unsigned char crcl = crc & 0xFF;
  unsigned char crch = (crc >> 8);
  //printf("FULL CRC = %04X, Len = %d, CRCH = %02X, CRCL = %02X\n", crc, len, crch, crcl);
  if (data[ len - 1 ] == crcl && data[ len ] == crch) {
    return true;
  }
  return false;
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
  case 5:
    return 57600;
  case 6:
    return 115200;
  default:
    return -1;
  }
}

bool transfer(unsigned char* command,int cmdsize,unsigned char* response,int responseSize,int sleepms,int pause) {
  unsigned short n;
  command[ 0 ] = cmdsize - 1;
  unsigned short crc = gencrc(command,cmdsize - 2);
  command[ cmdsize - 2 ] = crc & 0xFF;
  command[ cmdsize - 1 ] = (crc >> 8);

  if (Debug_) {
    printf("[transfer:send()]:>>\n");
    printarr(command,cmdsize);
    printf("<<[transfer:send()]\n");
  }
  //stopwatch(true);
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
    printf("[ERROR] Send command failed.");
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
    printf("[ERROR] Read command failed.");
    return false;
  }
  fflush(stdout);
  bool crcmatch = verifycrc(response);

  //printf("Read Finished in = %5.2f, size = %d\n", stopwatch(false), n);
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
  fflush(stdout);
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

int getresponse(unsigned char* response,int responseSize,int* len,unsigned char** data) {
  int i = 0;
  *len = response[ i++ ] - 5;
  //    unsigned char adr = response[i++];
  //    unsigned char cmd = response[i++];
  unsigned char status = response[ i++ ];

  *data = response + 4;
  return status;
}

static bool SetRegion(unsigned char region) {
  unsigned char maxfre;
  unsigned char minfre;
  switch (region) {
  case 'C':
    maxfre = 0x13;
    minfre = 0x40;
    break;
  case 'U':
    maxfre = 0x31;
    minfre = 0x80;
    break;
  case 'K':
    maxfre = 0x1F;
    minfre = 0xC0;
    break;
  case 'E':
    maxfre = 0x4E;
    minfre = 0x00;
    break;
  default:
    return false;
  }
  unsigned char command[] = { 0x06,0x00,0x22, maxfre, minfre, 0, 0 };
  unsigned char response[ 6 ];
  int retries = 10;
  do {
    bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
    if (!ok) {
      error_ = "FAILED to send write request.";
      continue;
    }
    if (response[ 3 ] == 0)
      return true;
  } while (retries--);
  return false;
}

static unsigned int GetSeriaNo() {
  unsigned char command[] = { 0x04,0x00,0x4C, 0, 0 };
  unsigned char response[ 10 ];
  bool ok = transfer(command,5,response,sizeof(response),10,20);
  if (!ok) {
    error_ = "FAILED to retrieve serial number.";
    return 0;
  }
  unsigned int SerialNo;
  SerialNo = (response[ 4 ] << 24) | (response[ 5 ] << 16) | (response[ 6 ] << 8) | (response[ 7 ]);
  return SerialNo;
}

static bool SetRfPower(unsigned char powerDbm) {
  unsigned char command[] = { 0x05,0x00,0x2F, powerDbm, 0, 0 };
  unsigned char response[ 6 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),100,20);
  if (!ok) {
    error_ = "FAILED to send write request.";
    return false;
  }
  return true;
}

static bool SetAddress(unsigned char ComAdrData) {
  unsigned char command[] = { 0x05,0x00,0x2F, ComAdrData, 0, 0 };
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
    unsigned char command[] = { 0x05,0x00,0x40, on, 0, 0 };
    unsigned char response[ 6 ];
    bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
    if (!ok)
      continue;
    return true;
  } while (retries--);
  return false;
}

static bool SetScanTime(unsigned char scantime) {
  unsigned char command[] = { 0x05,0x00,0x25, scantime, 0, 0 };
  unsigned char response[ 6 ];
  int retries = 10;
  do {
    bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
    if (!ok) {
      error_ = "FAILED to send write request.";
      continue;
    }
    scantime_ = scantime * 100;
    return true;
  } while (retries--);
  return false;
}

bool SetBaudRate(int baud) {
  baud = getBaudIndex(baud);
  unsigned char command[] = { 0x05,0x00,0x28, (unsigned char)baud, 0, 0 };
  unsigned char response[ 6 ];
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

bool GetSettings2(ReaderInfo* ri) {
  int retries = 10;
  int timeout = 0;
  do {
    unsigned char command[] = { 0x04,0x00,0x21, 0, 0 };
    unsigned char response[ 64 ];
    //    int retries = 10;
    bool ok;
    ok = transfer(command,sizeof(command),response,sizeof(response),10,10);

    //printarr(response, sizeof(response));
    if (command[ 2 ] != response[ 2 ])
      continue;

    /*
    if (n!=18) {
        char err[128];
        snprintf(err,sizeof(err),"Failed to load all reader info. Expecting 18 parameters, returned %d parameters.", n);
        //printf("Failed to load all reader info. Expecting 18 parameters, returned %d parameters.", n);
        error_ = err;
        continue;
    }
    */

    //    int status = response[3];
    ri->ComAdr = response[ 1 ];
    char* ver = ri->VersionInfo;
    ver[ 0 ] = response[ 4 ];
    ver[ 1 ] = response[ 5 ];
    ri->ReaderType = response[ 6 ];
    readertype_ = ri->ReaderType;
    ri->Protocol = response[ 7 ];
    int dmaxfre = response[ 8 ];
    int dminfre = response[ 9 ];
    ri->Power = response[ 10 ];
    ri->ScanTime = response[ 11 ];
    scantime_ = ri->ScanTime * 100;

    ((unsigned char*)&ri->Antenna)[ 0 ] = response[ 12 ];
    ri->BeepOn = response[ 13 ];
    // bbffffff
    ri->Band = ((dmaxfre >> 6) << 2) | (dminfre >> 6);
    ri->MinFreq = (dminfre & 0x3f);
    ri->MaxFreq = dmaxfre & 0x3f;

    if (ri->Protocol & 0x01) {
      ri->Protocol = 0x6B;
    }
    else if (ri->Protocol & 0x02) {
      ri->Protocol = 0x6C;
    }

    switch (ri->Band) {
    case 1: // Chinese Band
      ri->MinFreq = 920.125 * 1000;
      ri->MaxFreq = 920.125 * 1000 + ri->MaxFreq * 0.25 * 1000;
      ri->Band = 'C';
      break;
    case 2: // US Band
      ri->MinFreq = 902.75 * 1000;
      ri->MaxFreq = 902.75 * 1000 + ri->MaxFreq * 0.5 * 1000;
      ri->Band = 'U';
      break;
    case 3: // Korean Band
      ri->MinFreq = 917.1 * 1000;
      ri->MaxFreq = 917.1 * 1000 + ri->MaxFreq * 0.2 * 1000;
      ri->Band = 'K';
      break;
    case 4: // EU Band
      ri->MinFreq = 865.1 * 1000;
      ri->MaxFreq = 865.1 * 1000 + ri->MaxFreq * 0.2 * 1000;
      ri->Band = 'E';
      break;
    }

    ri->Serial = GetSeriaNo();
    ri->BaudRate = baud_;
    scantime_ = ri->ScanTime * 100;
    //printarr((unsigned char*)ri, sizeof(ReaderInfo));

    return true;
  } while (retries--);
  return false;
}

bool SetSettings2(ReaderInfo* ri) {
  ReaderInfo old;
  bool status = GetSettings2(&old);
  if (!status) {
    error_ = "Failed to retrieve settings.";
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
  /*
  if (old.ComAdr != ri->ComAdr) {
      // change bcomadr
      verify = SetAddress(ri->ComAdr);
  }
  */
  if (!verify) {
    return false;
  }
  // now verify settings
  ReaderInfo riv;
  status = GetSettings2(&riv);
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
  }


  if (ri->Band != riv.Band) {
    error_ += "Failed to verify Band settings. ";
    verify = false;
  }
  if (ri->BeepOn != riv.BeepOn) {
    error_ += "Failed to verify Beep settings. ";
    verify = false;
  }
  if (ri->Power != riv.Power) {
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

static bool clearbuff() {
  unsigned char command[] = { 0x04,0x00,0x73, 0, 0 };
  unsigned char response[ 7 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),0,5);
  if (!ok)
    return false;
  return (response[ 3 ] == 0);
}

static int gettagnum() {
  unsigned char command[] = { 0x04,0x00,0x74, 0, 0 };
  unsigned char response[ 8 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
  if (!ok) {
    error_ = "FAILED to send write request.";
    return 0;
  }
  if (response[ 3 ] == 0) {
    return (response[ 4 ] << 8) | response[ 5 ];
  }
  return 0;
}

/** \brief
 *  Gets Inventory, applies filter if specified.
 * \param port port name in the form of COM1, COM2 ... Baudrate must be one of the following:
 *     9600, 19200, 38400, 57600(default), 115200.
 * \return array which is multiple of dataLength. {type,ant,RSSI,count,epc[]}, where type, RSSI
 *     and count are int. epc array size is (dataLength - 4*4). The total array size will be even.
 *     If the operation fails, returns 0;
 */
int Inventory2(bool filter) {
  scan_.clear();
  bool success = InventoryNB2(scan_,filter);
  if (!success)
    return 0;

  return (unsigned)scan_.size();
}

/** \brief
 *  Gets Single Tag Inventory.
 * \return 1, if a single tag found, otherwise returns 0;
 */
int InventoryOne2() {
  scan_.clear();
  string cmd;
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x0F); // single inventory command
  cmd.push_back(0x00); // CRC placeholder
  cmd.push_back(0x00); // CRC placeholder

  unsigned char* command = (unsigned char*)cmd.data();
  unsigned char cmdlen = cmd.size();

  unsigned char response[ 1024 ];
  stopwatch(true);
  if (Debug_) {
    printf("[single-inventory:scan()]>>\n");
  }
  //platform_sleep(100);
  if (Debug_) {
    printf("Scantime = %d\n",scantime_);
  }
  bool ok = transfer(command,cmdlen,response,sizeof(response),scantime_ * 1.5,200); //scantime_ + 10);
  if (Debug_) {
    printf("<<[inventory:scan()]\n");
  }
  if (!ok)
    return false;
  int len;
  unsigned char* data = response + 4;

  if (response[ 3 ] == 0) {
    if (Debug_) {
      printf("[MSG] Status Check Failed.\n");
    }
    return false; // status check
  }

  int epcnt = response[ 5 ]; // read epc count and then point to the epc dat
  if (epcnt == 0) {
    if (Debug_) {
      printf("[MSG] No Tag Found.\n");
    }
    return false; // no tag found
  }

  for (int j = 0; j < epcnt; j++) {
    //        printarr(data, lenepc);
    ScanData sd;
    sd.ant = *data++;
    len = *data++;
    sd.data = string((char*)data,len);
    if (sd.data == string(len,'0'))
      continue; // EPC all zeros - invalid
    data += len;
    sd.type = MEM_EPC;
    sd.RSSI = 0;
    sd.count = 1;
    if (Debug_) {
      printf("[DBG] Ant = %d, Len = %d, RSSI = %d, Count = %d\n",sd.ant,len,sd.RSSI,sd.count);
    }
    scan_.push_back(sd);
  }

  return true;
}

bool GetResult2(unsigned char* scanresult,int index) {
  if (index >= (int)scan_.size())
    return false;
  ScanData sd = scan_[ index ];
  int epcLength = sd.data.size();
  //printf("Ant = %d, Len = %d, EPC = %s, RSSI = %d, Count = %d\n",
  //       sd.ant, sd.data.length(), tohex(sd.data).c_str(), sd.RSSI, sd.count);
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
  //memcpy((char*)sr->epc,(char*)sd.data.data(),epcLength);
  return true;
}

bool InventoryB2(vector<ScanData>& v,bool filter) {
  //    time_t t0;
  //    char str[100];

  //    t0 = clock();

  //    int count = 0;
  clearbuff();

  string cmd;
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x01); // direct inventory command
  //    cmd.push_back(0x18); // buffered inventory command

  cmd.push_back(q_); // QValue
  cmd.push_back(session_); // Session
  cmd.push_back(0x01); // Maskmem

  if (filter) {
    unsigned bitadr = filter_.MaskAdr * 8 + 32;
    cmd.push_back((unsigned char)(bitadr >> 8)); // MaskAdr
    cmd.push_back((unsigned char)(bitadr & 0xFF)); // MaskAdr
    cmd.push_back(filter_.MaskData.size() * 8); // MaskLen
    cmd += filter_.MaskData; // MaskData
  }
  else {
    cmd.push_back(0x00); // MaskAdr
    cmd.push_back(0x00); // MaskLen
    cmd.push_back(0x00); // MaskData
  }
  cmd.push_back(0x00); // AdrTID
  cmd.push_back(0x00); // LenTID

  cmd.push_back(0x00); // Target
  cmd.push_back(0x80); // Ant
  cmd.push_back(scantime_ / 100); // Scantime

  cmd.push_back(0x00); // CRC placeholder
  cmd.push_back(0x00); // CRC placeholder

  unsigned char* command = (unsigned char*)cmd.data();
  unsigned char cmdlen = cmd.size();

  unsigned char response[ 1024 ];
  printf("Scantime = %d\n",scantime_);
  stopwatch(true);
  bool ok = transfer(command,cmdlen,response,sizeof(response),10,scantime_ * 1.5);
  printarr(command,cmdlen);
  if (!ok)
    return false;

  printarr(response,response[ 0 ] + 1);
  //printf("Buffer Finished in %10.2f seconds.\n", elapsed(t0));
//    if (status != 1 && status != 3)
//        return false;
  int status = response[ 3 ];
  if (status == 0)
    return false;
  int ant = response[ 4 ];
  int epcnt = response[ 5 ]; // read epc count and then point to the epc data
  int len = response[ 6 ];
  unsigned char* data = &response[ 7 ];

  for (int j = 0; j < epcnt; j++) {
    //        printarr(data, lenepc);
    ScanData sd;
    sd.ant = ant;
    sd.data = string((char*)data,len);
    data += len;
    sd.type = MEM_EPC;
    sd.RSSI = 0;
    sd.count = 0;
    //printf("Ant = %d, Len = %d, RSSI = %d, Count = %d\n", sd.ant, len, sd.RSSI, sd.count);
    v.push_back(sd);
  }
  if (Debug_) {
    printf("Inventory Finished in = %5.2fs\n",stopwatch(false));
  }
  return true;
}

/**
 * Filter: x*, 2:x, x:2
 */
bool InventoryNB2(vector<ScanData>& v,bool filter) {
  emptyserial();

  clearbuff();

  string cmd;
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  //    cmd.push_back(0x01); // direct inventory command
  cmd.push_back(0x18); // buffered inventory command

  cmd.push_back(q_); // QValue
  cmd.push_back(session_); // Session

  if (filter) {
    // printf("Using filter...\n");
    cmd.push_back(0x01); // Maskmem

    unsigned bitadr = filter_.MaskAdr * 8 + 32;
    cmd.push_back((unsigned char)(bitadr >> 8)); // MaskAdr
    cmd.push_back((unsigned char)(bitadr & 0xFF)); // MaskAdr
    cmd.push_back(filter_.MaskData.size() * 8); // MaskLen
    cmd += filter_.MaskData; // MaskData

    //        cmd.push_back(0x00); // AdrTID
    //        cmd.push_back(0x00); // LenTID
    cmd.push_back(0x00); // Target
    cmd.push_back(0x00); // Ant
    cmd.push_back(scantime_ / 100); // Scantime
  }
  else {
    //        if (readertype_ != 15) {
    //            cmd.push_back(0x00); // MaskAdr
    //            cmd.push_back(0x00); // MaskLen
    //            cmd.push_back(0x00); // MaskData
    //        }
    cmd.push_back(0x00);
    cmd.push_back(0x80);
    cmd.push_back(scantime_ / 100); // Scantime
  }
  //    if (readertype_ == 15) {
  //        cmd.push_back(0x00);
  //        cmd.push_back(0x00);
  //        cmd.push_back(scantime_/100); // Scantime
  //    } else {
  //        cmd.push_back(0x00); // AdrTID
  //        cmd.push_back(0x00); // LenTID
  //        cmd.push_back(0x00); // Target
  //        cmd.push_back(0x80); // Ant
  //        cmd.push_back(scantime_/100); // Scantime
  //    }

  cmd.push_back(0x00); // CRC placeholder
  cmd.push_back(0x00); // CRC placeholder

  unsigned char* command = (unsigned char*)cmd.data();
  unsigned char cmdlen = cmd.size();

  unsigned char response[ 10 ];
  stopwatch(true);
  if (Debug_) {
    printf("[inventory:scan()]>>\n");
    fflush(stdout);
  }
  platform_sleep(100);
  printf("Scantime = %d\n",scantime_);
  //printarr(command, cmdlen);
  bool ok = transfer(command,cmdlen,response,sizeof(response),20,scantime_ * 1.5);
  if (Debug_) {
    printf("<<[inventory:scan()]\n");
    fflush(stdout);
  }
  //printf("Transfer Finished in = %5.2fs, size = %d\n", stopwatch(false), n);
  //printarr(command, cmdlen);
  if (!ok)
    return false;
  int len;
  unsigned char* data;
  //    int status =
  getresponse(response,sizeof(response),&len,&data);

  //    count = (data[0] << 8) | data[1];
  //    int tagnum = (data[2] << 8) | data[3];
      //printf("Scan Finished in %10.2f seconds.\n", elapsed(t0));
      //printf("Count = %d, Tag Num = %d\n", count, tagnum);

  //    int tagnum2 = gettagnum();

      // get buffer data
  unsigned char cmdbuf[] = { 0x04,0x00,0x72, 0, 0 };
  unsigned char bufresp[ 2048 ];
  memset(bufresp,0,sizeof(bufresp));
  bool repeat = true;
  int maxloop = 100;
  while (repeat) {
    int retries = 2;
    do {
      if (Debug_) {
        printf("[inventory:scan:bufresp()]>>\n");
      }
      bool ok = transfer(cmdbuf,sizeof(cmdbuf),bufresp,sizeof(bufresp),20,20);
      if (Debug_) {
        printf("<<[inventory:scan:bufresp()]\n");
      }
      //printarr(cmdbuf, sizeof(cmdbuf));
      if (!ok)
        continue;
      if (bufresp[ 3 ] == 0x01) {
        repeat = false;
        break;
      }
      if (bufresp[ 2 ] == 0x72)
        break;
    } while (retries--);
    //status =
    getresponse(bufresp,sizeof(bufresp),&len,&data);

    //printf("Buffer Finished in %10.2f seconds.\n", elapsed(t0));
    //    if (status != 1 && status != 3)
    //        return false;
    if (bufresp[ 6 ] == bufresp[ 7 ] && bufresp[ 0 ] == 0)
      return false;
    len -= 2;
    int epcnt = *data++; // read epc count and then point to the epc data

    if (epcnt == 0) {
      printf("No tags found.\n");
    }
    for (int j = 0; j < epcnt; j++) {
      //        printarr(data, lenepc);
      ScanData sd;
      sd.ant = *data++;
      len = *data++;
      sd.data = string((char*)data,len);
      if (sd.data == string(len,'0'))
        continue; // EPC all zeros - invalid
      data += len;
      sd.type = MEM_EPC;
      char rssi = *data++;
      sd.RSSI = rssi;
      sd.count = *data++;
      if (Debug_) {
        printf("[DBG] Ant = %d, Len = %d, RSSI = %d, Count = %d\n",sd.ant,len,sd.RSSI,sd.count);
      }
      v.push_back(sd);
    }
    maxloop--;
    if (!maxloop)
      break;
  }
  if (Debug_) {
    printf("Inventory Finished in = %5.2fs\n",stopwatch(false));
  }
  fflush(stdout);
  clearbuff();
  return true;
}

// TODO - Does not return TID inventory,
// returns EPC inventory instead.
bool InventoryTID2(vector<ScanData>& v) {
  emptyserial();
  clearbuff();
  unsigned char command[] = {
      0x00, 0x00, 0x18,
      0x02, // QValue
      0x00, // Session
      0x02, // Maskmem
      0x00, // MaskAdr
      0x00, // MaskLen
      0x00, // MaskData
      0x00, // AdrTID
      6,    // LenTID
      0x00, // Target
      0x80, // Ant
      0x05, // Scantime
      0,0
  };
  unsigned char response[ 1024 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),100,100);
  if (!ok)
    return false;
  int len;
  unsigned char* data;
  int status = getresponse(response,sizeof(response),&len,&data);

  //    int count = (data[0] << 8) | data[1];
  //    int tagnum = (data[2] << 8) | data[3];
      //printf("Count = %d, Tag Num = %d\n", count, tagnum);

  //    int tagnum2 = gettagnum();

      // get buffer data
  unsigned char cmdbuf[] = { 0x04,0x00,0x72, 0, 0 };
  unsigned char bufresp[ 2048 ];
  ok = transfer(cmdbuf,sizeof(cmdbuf),bufresp,sizeof(bufresp),10,10);
  if (!ok)
    return false;
  status = getresponse(bufresp,sizeof(bufresp),&len,&data);
  if (status != 1)
    return false;
  len -= 2;
  int epcnt = *data++; // read epc count and then point to the epc data

  for (int j = 0; j < epcnt; j++) {
    //        printarr(data, lenepc);
    ScanData sd;
    sd.ant = *data++;
    len = *data++;
    sd.data = string((char*)data,len);
    sd.type = MEM_EPC;
    data += len;
    char rssi = *data++;
    sd.RSSI = rssi;
    sd.count = *data++;
    //printf("Ant = %d, Len = %d, RSSI = %d, Count = %d\n", sd.ant, len, sd.RSSI, sd.count);
    v.push_back(sd);
  }
  return true;
}

bool Auth2(unsigned char* password,unsigned char size) {
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

bool SetPassword2(unsigned char* epc,unsigned char epcLen,unsigned char* pass,unsigned char size) {
  if (size != 4) {
    error_ = "Password must be 4 bytes long.";
    return false;
  }

  //    return WriteWord((unsigned char*)epc,epcLen,pass,3,MEM_PASSWORD);
  return Write((unsigned char*)epc,epcLen,pass,size / 2,2,MEM_PASSWORD);
}

bool SetKillPassword2(unsigned char* epc,unsigned char epcLen,unsigned char* pass,unsigned char size) {
  return Write((unsigned char*)epc,epcLen,pass,size / 2,0,MEM_PASSWORD);
}

//bool GetKillPassword(unsigned char *epc, int epclen, unsigned char *pass, unsigned char *passlen) {
//    *passlen = 8;
//    return Read((unsigned char *)epc, epclen, pass, 4, 0, MEM_PASSWORD);
//}

bool Write2(unsigned char* epc,unsigned char epcLen,unsigned char* data,int wsize,int windex,int memtype) {
  //cout << "Password: " << tohex(password_) << endl;
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

  char* password = (char*)password_.data();
  //    unsigned char command[] = {0x05,0x00,0x03,
  //        (unsigned char)(size/2), epcLen/2,
  //        password[3], password[2], password[1], password[0]};
  //    unsigned char cc[sizeof(command) + epcLen + 2];
  string command;
  command.push_back(0x00); // number of bytes following this byte
  command.push_back(0x00); // address - always zero
  command.push_back(0x03); // command
  command.push_back(wsize); // number of words to be written
  command.push_back((char)epcLen / 2); // number of words in epc
  command += string((char*)epc,epcLen); // epc data
  command.push_back(memtype); // memory type to be written
  if (memtype == MEM_EPC) {
    command.push_back(2 + windex); // offset is 2 for EPC memory
  }
  else {
    command.push_back(windex); // no offset for other than EPC memory
  }
  command += string((char*)data,wsize * 2); // data to be written
  command.push_back(password[ 3 ]); // password lsb
  command.push_back(password[ 2 ]); // password
  command.push_back(password[ 1 ]); // password
  command.push_back(password[ 0 ]); // password msb
  command.push_back(0); // crc place holder
  command.push_back(0); // crc place holder

  //cout << "Command = " << tohex(command) << endl;

//    unsigned char response[32];
  int retries = 10;
  do {
    unsigned char response[ 6 ];
    bool ok = transfer((unsigned char*)command.data(),command.size(),response,sizeof(response),150,100);
    //printarr((unsigned char*)command.data(),command.size());
    //n = transfer((unsigned char*)command.data(), command.size(), response, sizeof(response), delay);
    if (!ok) {
      error_ = "FAILED to send write request.";
      return false;
    }
    char result = (response[ 3 ] == 0);
    if (result) {
      return true;
    }
  } while (retries--);
  error_ = "FAILED to write.";
  return false;
}

void LibVersion(unsigned char* version) {
  version[ 0 ] = (LIB_VERSION >> 8) & 0x00FF;
  version[ 1 ] = LIB_VERSION & 0x00FF;
}

bool WriteWord2(unsigned char* epc,unsigned char epcLen,unsigned char* data,unsigned char windex,int memtype) {
  //    do {
  bool success = Write(epc,epcLen,data,1,windex,memtype);
  if (memtype == MEM_EPC)
    return success;
  if (success) {
    unsigned char data2[ 2 ] = { 0,0 };
    success = ReadWord(epc,epcLen,data2,windex,memtype);
    if (success) {
      if (data[ 0 ] == data2[ 0 ] && data[ 1 ] == data2[ 1 ])
        return true; // data successfully written.
    }
  }
  //    } while(retries--);
  return false;
}

bool Read2(unsigned char* epc,unsigned epclen,unsigned char* data,int wnum,int windex,int memtype) {
  //cout << "Password: " << tohex(password_) << endl;
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

  char* password = (char*)password_.data();
  //    unsigned char command[] = {0x05,0x00,0x03,
  //        (unsigned char)(size/2), epcLen/2,
  //        password[3], password[2], password[1], password[0]};
  //    unsigned char cc[sizeof(command) + epcLen + 2];
  string command;
  if (memtype == MEM_EPC) {
    windex += 2;
  }
  command.push_back(0x00);
  command.push_back(0x00);
  command.push_back(0x02);
  command.push_back((char)epclen / 2);
  command += string((char*)epc,epclen);
  command.push_back(memtype);
  command.push_back(windex);
  command.push_back(wnum);
  command.push_back(password[ 3 ]);
  command.push_back(password[ 2 ]);
  command.push_back(password[ 1 ]);
  command.push_back(password[ 0 ]);
  command.push_back(0); // crc place holder
  command.push_back(0); // crc place holder

  //cout << "Command = " << tohex(command) << ", Size = " << command.size() << endl;
  unsigned char response[ 128 ];
  int result = -100;
  int retries = 10;
  do {
    bool ok = transfer((unsigned char*)command.data(),command.size(),response,sizeof(response),50,50);
    //printarr((unsigned char*)command.data(),command.size());
    if (!ok) {
      error_ = "FAILED to send write request. ";
      return false;
    }
    result = response[ 3 ];
    //printf("result: %d\n", result);
    if (result == 0) {
      memcpy(data,&response[ 4 ],wnum * 2);
      return true;
    }
  } while (retries--);

  error_ = "FAILED to read.";
  return false;
}

bool ReadWord2(unsigned char* epc,unsigned char epclen,unsigned char* data,int windex,int memtype) {
  return Read(epc,epclen,data,1,windex,memtype);
}

/**
 * filterDesc = [~][-][.]aa>>[mm=]value[=nn]<<bb[~][-][.]
 *   if aa: is provided, then the value is right shifted this many bits.
 *   if :bb is provided, then the value is left shifted this many bits.
 *   both aa and bb cannot be specified together.
 *   if neither aa or bb is provided, then value or value is used as prefix.
 *   if *value is provided, then value is used as suffix.
 *   if ---value, where each - indicate 4 bit nibble.
 *   if value---, where each - indicate 4 bit nibble.
 *   value... where each . indicate a bit.
 *   ...value where each . indicate a bit.
 *   ~~~value where each ~ indicate a byte.
 *   value~~~ where each ~ indicate a byte.
 */
bool CreateFilter(char* filterDesc) {
  return false;
}

bool SetFilter2(int maskAdrInByte,int maskLenInByte,unsigned char* maskDataByte) {
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

bool WriteMemWord2(unsigned char* epc,unsigned char epclen,unsigned char* data,unsigned char windex) {
  bool b = WriteWord(epc,epclen,data,windex,MEM_USER);
  if (!b) {
    unsigned char data2[ 2 ] = { 0,0 };
    //printarr(epc,epcLen);
    //printarr(epc2,epcLen);
    bool b = ReadMemWord(epc,epclen,data2,windex);
    //printarr(data,2);
    //printarr((unsigned char*)data2,2);
    if (b) {
      if (data[ 0 ] == data2[ 0 ] && data[ 1 ] == data2[ 1 ]) {
        return true;
      }
    }
  }

  return b;
}

bool WriteEpcWord2(unsigned char* epc,unsigned char epclen,unsigned char* data,unsigned char windex) {
  bool b = WriteWord(epc,epclen,data,windex,MEM_EPC);
  if (!b) {
    unsigned char epc2[ 32 ];
    memcpy(epc2,epc,epclen);
    epc2[ windex * 2 ] = data[ 0 ];
    epc2[ windex * 2 + 1 ] = data[ 1 ];
    unsigned char data2[ 2 ] = { 0,0 };
    //printarr(epc,epcLen);
    //printarr(epc2,epcLen);
    bool b = ReadEpcWord(epc2,epclen,data2,windex,MEM_EPC);
    //printarr(data,2);
    //printarr((unsigned char*)data2,2);
    if (b) {
      if (data[ 0 ] == data2[ 0 ] && data[ 1 ] == data2[ 1 ]) {
        return true;
      }
    }
  }
  return b;
}

bool WriteEpc2(unsigned char* epc,unsigned char epcLen,unsigned char* data) {
  bool ok = Write(epc,epcLen,data,epcLen / 2,0,MEM_EPC);
  if (ok) {
    unsigned char newepc[ epcLen ];
    ok = Read(data,epcLen,newepc,epcLen / 2,0,MEM_EPC);
    if (memcmp(data,newepc,epcLen) == 0) {
      printf("EPC verified.\n");
      return true;
    }
  }
  return false;
}

bool ReadMemWord2(unsigned char* epc,unsigned char epclen,unsigned char* data,unsigned char windex) {
  return ReadWord(epc,epclen,data,windex,MEM_USER);
}

bool ReadEpcWord2(unsigned char* epc,unsigned char epclen,unsigned char* data,int windex,int memtype) {
  return Read(epc,epclen,data,1,windex,MEM_EPC);
}

bool TagExists(unsigned char* epc,unsigned char epclen) {
  Filter f = filter_; // backup filter
  bool success = SetFilter(0,epclen,epc);
  if (!success) {
    filter_ = f;
    return false;
  }
  int n = Inventory(true);
  //printf("n=%d\n", n);
  filter_ = f;
  return (n > 0) ? 1 : 0;
}

bool Kill() {
  return false;
}

bool Lock() {
  return false;
}

bool Erase() {
  return false;
}

bool Is496Bits() {
  unsigned char command[] = { 0x05,0x00,0x71, 0, 0 };
  unsigned char response[ 7 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),100,10);
  if (!ok) {
    error_ = "FAILED to send write request.";
    return false;
  }
  if (response[ 3 ] == 0) {
    return (response[ 4 ] != 0);
  }
  error_ = "FAILED to get bit info.";
  return false;
}

bool Set496Bits(bool bits496) {
  unsigned char command[] = { 0x05,0x00,0x70, bits496, 0, 0 };
  unsigned char response[ 6 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),5,10);
  if (!ok) {
    error_ = "FAILED to send write request.";
    return false;
  }
  if (response[ 3 ] == 0)
    return true;
  return false;
}

bool SetPassword(unsigned char* epc,unsigned char epcLen,unsigned char* pass,unsigned char size) {
  if (size != 4) {
    error_ = "Password must be 4 bytes long.";
    return false;
  }

  //    return WriteWord((unsigned char*)epc,epcLen,pass,3,MEM_PASSWORD);
  return Write((unsigned char*)epc,epcLen,pass,size / 2,2,MEM_PASSWORD);
}

bool SetKillPassword(unsigned char* epc,unsigned char epcLen,unsigned char* pass,unsigned char size) {
  return Write((unsigned char*)epc,epcLen,pass,size / 2,0,MEM_PASSWORD);
}

bool GetKillPassword(unsigned char* epc,int epclen,unsigned char* pass,unsigned char* passlen) {
  *passlen = 8;
  return Read((unsigned char*)epc,epclen,pass,4,0,MEM_PASSWORD);
}

bool GetTID2(unsigned char* epc,unsigned char epclen,unsigned char* tid,unsigned char* tidlen) {
  *tidlen = epclen;
  error_ = "";
  emptyserial();
  bool success = Read(epc,epclen,tid,epclen / 2,0,MEM_TID);
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

bool SetGPO(unsigned char gpoNumber) {
  // gpoNumber should be either 1 for GPO1 or 2 for GPO2
  // if gpoNumber == 3, then both output will be set
  unsigned char command[] = { 0x05,0x00,0x46, gpoNumber, 0, 0 };
  unsigned char response[ 6 ];
  bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
  //printf("Response:\n");
  //printarr(response, sizeof(response));
  if (!ok) {
    error_ = "FAILED to send write request.";
    return false;
  }
  if (response[ 3 ] == 0)
    return true;
  return false;
}

char GetGPI(unsigned char gpiNumber) {
  // gpiNumber should be either 1 for GP1 or 2 for GPi2
  // if gpiNumber == 3, state is invalid.
  // if gpiNumber = 4 or 5, then GPO1 and GPO2 values will be returned respectively.
  unsigned char command[] = { 0x05,0x00,0x47, 0, 0 };
  unsigned char response[ 7 ];
  int retries = 10;
  do {
    //printf("INPUT:\n");
    bool ok = transfer(command,sizeof(command),response,sizeof(response),10,10);
    //printarr(response, 7);
    if (!ok) {
      error_ = "FAILED to send write request.";
      return -1;
    }
    if (response[ 3 ] == 0) {
      unsigned char output = response[ 4 ] & 0x03;
      output = output & gpiNumber;
      output >>= (gpiNumber - 1);
      //printf("in=%d\n",output);
      return output;
    }
  } while (retries--);
  return -1;
}

void LastError(char* buffer) {
  strcpy(buffer,error_.c_str());
}

unsigned int little2big(unsigned int little) {
  unsigned int b0 = little & 0x000000ff;
  unsigned int b1 = (little >> 8) & 0xff;
  unsigned int b2 = (little >> 16) & 0xff;
  unsigned int b3 = (little >> 24) & 0xff;
  return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

/*
    Serial : 4 byte int
    VersionInfo : 2 byte {major, minor}
    Antenna : 1 byte
    ComAdr : 1 byte
    ReaderType : 1 byte
    Protocol : 1 byte
    Band : 1 byte
    Power : 1 byte
    ScanTime : 4 byte int
    BeepOn : 1 byte
    Reserved1 : 1 byte
    Reserved2 : 1 byte
    MaxFreq : 4 byte int
    MinFreq : 4 byte int
    BaudRate : 4 byte int

struct ReaderInfo {
    // the following constants are reader constants
    // they cannot be changed.
    int Serial = 0;
    char VersionInfo[2] = {0,0};
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

    unsigned int MaxFreq  = 0;
    unsigned int MinFreq  = 0;
    unsigned int BaudRate = 0;
};

*/

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

}
