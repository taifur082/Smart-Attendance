#include "common.h"
#include <stdio.h>
#include <inttypes.h>
#include <string>
#include "driver.h"

void printsettings(ReaderInfo* ri) {
  printf("\n>>Settings>>\n");
  printf("Version: %d.%d\n",ri->VersionInfo[ 0 ],ri->VersionInfo[ 1 ]);
  printf("Serial: %d\n",ri->Serial);
  printf("ComAdr: %d\n",ri->ComAdr);
  printf("Reader Type: %d\n",ri->ReaderType);
  printf("Protocol: %X\n",ri->Protocol);
  printf("Band: %c\n",ri->Band);
  printf("Min Freq: %fMHz\n",ri->MinFreq / 1000.0);
  printf("Max Freq: %fMHz\n",ri->MaxFreq / 1000.0);
  printf("Power: %ddB\n",ri->Power);
  printf("Scan Time: %dms\n",ri->ScanTime * 100);
  printf("Antenna: %d\n",ri->Antenna);
  printf("BeepOn: %d\n",ri->BeepOn);
  printf("Baud Rate: %d\n",ri->BaudRate);
  fflush(stdout);
}

void printarr(unsigned char* arr,int size) {
  for (int i = 0; i < size; i++) {
    printf("%02X ",arr[ i ]);
  }
  printf("\n");
  fflush(stdout);
}

double  stopwatch(bool start) {
  /*
      static time_t t0;
      if (start) {
          t0 = clock();
          return 0.0;
      }
      time_t t1 = clock();
      double e = (double)(t1-t0)/((double)CLK_TCK);
      return e;
  */
  struct timeval tt;
  static timeval st;
  double secs = 0;

  if (start) {
    gettimeofday(&st,NULL);
    return 0.0;
  }

  gettimeofday(&tt,NULL);
  // Do stuff  here
  secs = (double)(tt.tv_usec - st.tv_usec) / 1000000.0 + (double)(tt.tv_sec - st.tv_sec);;
  printf("time taken %f\n",secs);
  return secs;
}

string tohex(string& uid)
{
  string z = "";
  char p[ 3 ] = "00";
  for (int i = 0; i < (int)uid.size(); i++)
  {
    snprintf(p,sizeof(p),"%02X",uid[ i ] & 0xFF);
    z += p;
  }
  return z;
}


string bytes2hex(unsigned char* uid,int n)
{
  string z = "";
  char p[ 3 ] = "00";
  for (int i = 0; i < n; i++)
  {
    snprintf(p,sizeof(p),"%02X",uid[ i ] & 0xFF);
    z += p;
  }
  return z;
}

void toUpperCase(std::string str)
{
  for (int i = 0; i < (int)str.length(); i++)
  {
    char c = str[ i ];
    c = toupper(c);
    str.replace(i,1,1,c);
  }
}

void getBytes(std::string str,unsigned char* buf,size_t len)
{
  memcmp(str.data(),buf,len);
}

string hex2bytes(string hex)
{
  // cout << "HEX: " << hex << endl;
  string bytes;
  for (unsigned int i = 0; i < hex.length(); i += 2)
  {
    std::string byteString = hex.substr(i,2);
    // cout << byteString << endl;
    char byte = (char)strtol(byteString.c_str(),NULL,16);
    bytes.push_back(byte);
  }
  return bytes;
}

unsigned long millis()
{
  struct timeval time;
  gettimeofday(&time,NULL);
  unsigned long s1 = (unsigned long)(time.tv_sec) * 1000;
  unsigned long s2 = (time.tv_usec / 1000);
  return s1 + s2;
}

void printtags(int n)
{
  for (int i = 0; i < n; i++)
  {
    ScanResult sd;
    GetResult((unsigned char*)&sd,i);
    string epc = string((char*)sd.epc,sd.epclen);
    string hex = tohex(epc);
    printf("Ant = %d, Len = %d, EPC = %s, RSSI = %d, Count = %d\n",
      sd.ant,sd.epclen,hex.c_str(),sd.RSSI,sd.count);
  }
}

int ultoa_(uint32_t num,char* buf)
{
  uint32_t rem;
  uint32_t div = num;
  int i = 0;
  while (div > 0)
  {
    rem = div % 10;
    div /= 10;
    buf[ i++ ] = (char)rem;
    printf("rem=%" PRIu32 "\n",rem);
  }
  return i;
}

void printtag(int n)
{
  for (int i = 0; i < n; i++)
  {
    ScanResult sd;
    GetResult((unsigned char*)&sd,i);
    string epc = string((char*)sd.epc,sd.epclen);
    string hex = tohex(epc);
    printf("Ant = %d, Len = %d, EPC = %s, RSSI = %d, Count = %d\n",
      sd.ant,sd.epclen,hex.c_str(),sd.RSSI,sd.count);
    unsigned char tid[ 16 ];
    memset(tid,0,sizeof(tid));
    unsigned char tidlen = 0;
    bool success = GetTID((unsigned char*)epc.data(),epc.size(),tid,&tidlen);
    if (success)
    {
      TagInfo info;
      GetTagInfo(tid,&info);
      printf("Type=%s\n",info.chip);
      string t = string((char*)tid,tidlen);
      printf("TID = %s\n",tohex(t).c_str());
    }
    else
    {
      printf("TID could not be retrieved.\n");
    }

    //        unsigned char data[] = {0x11, 0x11};
    //        unsigned char result = WriteMemWord((unsigned char*)epc.data(),epc.size(), data, 0);
    //        if (!result) {
    //            printf("Failed to write EPC.\n");
    //        }
    //        unsigned char result = WriteEpcWord((unsigned char*)epc.data(),epc.size(), data, 0);
    //        if (!result) {
    //            printf("Failed to write EPC.\n");
    //        }
    /*
            unsigned char data[2] = {0,0};
            int epcsize = 0;
            string newepc = hex2bytes("BEEDFEEDABBACADE12349876");
            printf("New EPC HEX = %s\n", tohex(newepc).c_str());
    //        ReadWord((unsigned char*)epc.data(), epc.size(), (unsigned char*)data, -2, MEM_EPC);
    //        ReadWord((unsigned char*)epc.data(), epc.size(), (unsigned char*)&epcsize, -2, MEM_EPC);

            unsigned char d2[] = {0x12, 0x34};
            WriteMemWord((unsigned char*)epc.data(), epc.size(), d2, 0);
            unsigned char d3[] = {0x00, 0x00};
            ReadMemWord((unsigned char*)epc.data(), epc.size(), d3, 0);
            if (!SetEPC((unsigned char*)epc.data(), epc.size(), (unsigned char*)newepc.data())) {
                printf("FAILED to write EPC.\n");
            }
    */
  }
}