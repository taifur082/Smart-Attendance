// #include <iostream>
// #include <stdio.h>
// #include "driver.h"
// #include "common.h"
// #include <string.h>

// #ifdef _WIN32
// // Windows-specific code here
// #include <Windows.h>
// #include <WinDef.h>
// #endif

// using namespace std;

// string tohex(string &uid)
// {
//     string z = "";
//     char p[3] = "00";
//     for (int i = 0; i < (int)uid.size(); i++)
//     {
//         snprintf(p, sizeof(p), "%02X", uid[i] & 0xFF);
//         z += p;
//     }
//     return z;
// }

// string bytes2hex(unsigned char *uid, int n)
// {
//     string z = "";
//     char p[3] = "00";
//     for (int i = 0; i < n; i++)
//     {
//         snprintf(p, sizeof(p), "%02X", uid[i] & 0xFF);
//         z += p;
//     }
//     return z;
// }

// void toUpperCase(string &str)
// {
//     for (int i = 0; i < (int)str.length(); i++)
//     {
//         char c = str[i];
//         c = toupper(c);
//         str.replace(i, 1, 1, c);
//     }
// }

// void getBytes(string &str, unsigned char *buf, size_t len)
// {
//     memcmp(str.data(), buf, len);
// }

// string hex2bytes(string hex)
// {
//     // cout << "HEX: " << hex << endl;
//     string bytes;
//     for (unsigned int i = 0; i < hex.length(); i += 2)
//     {
//         std::string byteString = hex.substr(i, 2);
//         // cout << byteString << endl;
//         char byte = (char)strtol(byteString.c_str(), NULL, 16);
//         bytes.push_back(byte);
//     }
//     return bytes;
// }

// unsigned long millis()
// {
//     struct timeval time;
//     gettimeofday(&time, NULL);
//     unsigned long s1 = (unsigned long)(time.tv_sec) * 1000;
//     unsigned long s2 = (time.tv_usec / 1000);
//     return s1 + s2;
// }

// void printtags(int n)
// {
//     for (int i = 0; i < n; i++)
//     {
//         ScanResult sd;
//         GetResult((unsigned char *)&sd, i);
//         string epc = string((char *)sd.epc, sd.epclen);
//         string hex = tohex(epc);
//         printf("Ant = %d, Len = %d, EPC = %s, RSSI = %d, Count = %d\n",
//                sd.ant, sd.epclen, hex.c_str(), sd.RSSI, sd.count);
//     }
// }

// int ultoa_(uint32_t num, char *buf)
// {
//     uint32_t rem;
//     uint32_t div = num;
//     int i = 0;
//     while (div > 0)
//     {
//         rem = div % 10;
//         div /= 10;
//         buf[i++] = (char)rem;
//         printf("rem=%d\n", rem);
//     }
//     return i;
// }

// void printtag(int n)
// {
//     for (int i = 0; i < n; i++)
//     {
//         ScanResult sd;
//         GetResult((unsigned char *)&sd, i);
//         string epc = string((char *)sd.epc, sd.epclen);
//         string hex = tohex(epc);
//         printf("Ant = %d, Len = %d, EPC = %s, RSSI = %d, Count = %d\n",
//                sd.ant, sd.epclen, hex.c_str(), sd.RSSI, sd.count);
//         unsigned char tid[16];
//         memset(tid, 0, sizeof(tid));
//         unsigned char tidlen = 0;
//         bool success = GetTID((unsigned char *)epc.data(), epc.size(), tid, &tidlen);
//         if (success)
//         {
//             TagInfo info;
//             GetTagInfo(tid, &info);
//             printf("Type=%s\n", info.chip);
//             string t = string((char *)tid, tidlen);
//             printf("TID = %s\n", tohex(t).c_str());
//         }
//         else
//         {
//             printf("TID could not be retrieved.\n");
//         }

//         //        unsigned char data[] = {0x11, 0x11};
//         //        unsigned char result = WriteMemWord((unsigned char*)epc.data(),epc.size(), data, 0);
//         //        if (!result) {
//         //            printf("Failed to write EPC.\n");
//         //        }
//         //        unsigned char result = WriteEpcWord((unsigned char*)epc.data(),epc.size(), data, 0);
//         //        if (!result) {
//         //            printf("Failed to write EPC.\n");
//         //        }
//         /*
//                 unsigned char data[2] = {0,0};
//                 int epcsize = 0;
//                 string newepc = hex2bytes("BEEDFEEDABBACADE12349876");
//                 printf("New EPC HEX = %s\n", tohex(newepc).c_str());
//         //        ReadWord((unsigned char*)epc.data(), epc.size(), (unsigned char*)data, -2, MEM_EPC);
//         //        ReadWord((unsigned char*)epc.data(), epc.size(), (unsigned char*)&epcsize, -2, MEM_EPC);

//                 unsigned char d2[] = {0x12, 0x34};
//                 WriteMemWord((unsigned char*)epc.data(), epc.size(), d2, 0);
//                 unsigned char d3[] = {0x00, 0x00};
//                 ReadMemWord((unsigned char*)epc.data(), epc.size(), d3, 0);
//                 if (!SetEPC((unsigned char*)epc.data(), epc.size(), (unsigned char*)newepc.data())) {
//                     printf("FAILED to write EPC.\n");
//                 }
//         */
//     }
// }

// void scan()
// {
//     vector<ScanData> v;
//     bool ok = InventoryNB(v, false);
//     if (!ok)
//     {
//         return;
//     }
//     printf("Found %lu entries\n", v.size());
//     for (int i = 0; i < (int)v.size(); i++)
//     {
//         ScanData sd = v[i];
//         string hex = tohex(sd.data);
//         printf("Ant = %d, Len = %lu, EPC = %s, RSSI = %d, Count = %d\n",
//                (int)sd.ant, sd.data.length(), tohex(sd.data).c_str(), (int)sd.RSSI, sd.count);
//     }
// }

// void inventory_one()
// {

//     bool ret = InventoryOne(); // @suppress("Invalid arguments")
//     /*
//     for(int i=0;i<(int)scan_.size();i++) {
//         ScanData sd = scan_[i];
//         string hex = tohex(sd.data);
//         printf("Ant = %d, Len = %lu, EPC = %s, RSSI = %d, Count = %d\n",
//                (int)sd.ant, sd.data.length(), tohex(sd.data).c_str(), (int)sd.RSSI, sd.count);
//         //unsigned char pass[] = {0,0,0,0};
//         //SetPassword((unsigned char*)sd.data.data(), sd.data.length(), pass, 4);
//     }
//     */
//     CloseComPort();
//     exit(1);
// }

// void write_epc()
// {

//     printf("Attempting to write EPC.\n");

//     // ABCD1234EF56A1B2C3DDBB44
//     unsigned char epc[] = {0xAB, 0xCD, 0x12, 0x34, 0xEF, 0x56, 0xA1, 0xB2, 0xC3, 0xDD, 0xBB, 0x44};
//     unsigned char newepc[] = {0xAB, 0xCD, 0x12, 0x34, 0xEF, 0x56, 0xA1, 0xB2, 0xC3, 0xDD, 0xBB, 0x44};
//     unsigned char data[12];
//     Read(epc, 12, data, 6, 0, MEM_EPC);
//     char hex[43];
//     string str = bytes2hex(data, 12);
//     printf("hex = %s\n", str.c_str());

//     bool ok = WriteEpc(epc, 12, newepc);
//     if (ok)
//     {
//         printf("EPC written.\n");
//     }
//     else
//     {
//         printf("Failed to write EPC.\n");
//     }

//     CloseComPort();
//     exit(2);
// }

// int main()
// {

//     extern bool Debug_;
//     Debug_ = true;

//     char ports[256];
//     int n = AvailablePorts(ports);
//     printf("n = %d, ports=%s\n", n, ports);

//     extern void nano_main();
//     // nano_main();
//     //    char port[] = "ttyACM0"; // tty.usbmodem2201";
//     char port[] = "com3";

//     //    unsigned char comaddr = 0;
//     printf("Using Serial port: %s\n", port);
//     bool err = OpenComPort(port, 115200);
//     if (!err)
//     {
//         printf("Failed to connect.\n");
//         return 1;
//     }
//     printf("Connection successful.\n");
//     bool success;

//     ReaderInfo ri;
//     success = GetSettings(&ri);
//     if (!success)
//     {
//         printf("GetSettings() failed.\n");
//         return 1;
//     }
//     printsettings(&ri);
//     /*
//         bool retval = LoadSettings((unsigned char*)ports);
//     //    ri.BaudRate = 115200;
//         ri.ScanTime = 30;
//         retval = SaveSettings((unsigned char*)&ri);
//         success = GetSettings(&ri);
//         if (!success) {
//             printf("GetSettings() failed.\n");
//             return 1;
//         }
//         printsettings(&ri);
//         scan();
//     */

//     //    while(1) {
//     //        printf("in=%d\n",(int)GetGPI(1));
//     //        platform_sleep(1000);
//     //    }

//     //    ri.ScanTime = 5;

//     // write_epc();
//     //    inventory_one();

//     //=========================TESTING CODEFOR GET_SETTINGS===================================================================

//     //    ri.Band = 'U';
//     //    ri.Power = 25;
//     //    ri.ScanTime = 30;
//     //    ri.BaudRate = 57600;
//     //    success = SetSettings(&ri);
//     //    if (!success) {
//     //        printf("SetSettings() failed.\n");
//     //        return 1;
//     //    }

//     //=========================TESTING CODEFOR GET_SETTINGS=====================================================================
//     //    success = GetSettings(&ri);
//     //    if (!success) {
//     //        printf("GetSettings() failed.\n");
//     //        return 1;
//     //    }
//     //    printsettings(&ri);

//     //=========================TESTING CODE FOR INVENTORY BLOCKING=============================================================

//     vector<ScanData> v;
//     InventoryB(v, false);

//     //=========================TESTING CODE FOR GET_TID and READ================================================================
//     //    unsigned char TID[12];
//     //    unsigned char EPC[12] = {0x33, 0x30, 0xAF, 0xEC, 0x2B, 0x01, 0x15, 0xC0, 0x00, 0x00, 0x00, 0x01};
//     //    unsigned char epclen;
//     //    GetTID(EPC, (unsigned char)0x0C, TID, &epclen);
//     //    printf("TID FROM MAIN: ");
//     //    printarr(TID, sizeof(TID));

//     //=========================TESTING CODE FOR WRITE=========================================================================
//     // bool WriteMemWord(unsigned char *epc, unsigned char epclen, unsigned char *data, unsigned char windex)
//     //    unsigned char EPC[12] = {0x33, 0x30, 0xAF, 0xEC, 0x2B, 0x01, 0x15, 0xC0, 0x00, 0x00, 0x00, 0x01};
//     //    unsigned char epclen = sizeof(EPC);
//     //    unsigned char data[2] = {0x12, 0xAD};
//     //    WriteMemWord(EPC, epclen, data, 0x00); // invokes write word -> write()

//     //=========================TESTING CODE FOR INVENTORY_ONE=========================================================================
//     //    int status = InventoryOne();
//     //    printf("InventoryOne Status: %d\n", status);

//     //=========================TESTING CODE FOR INVENTORY_NB=========================================================================
//     //     printf(">>>>>>>>>Inventory Started<<<<<<<<<<<<\n");
//     //     int nt = Inventory(false);
//     //     if(nt){
//     //         printf(">>>>>>>>>>>>>Inventory is successful<<<<<<<<<<<<<<<\n");
//     //         printf("TAG COUNT: %d\n", nt);
//     //      printtags(n);
//     //     }

//     //=========================TESTING CODE FOR SET_SCAN_TIME=========================================================================
//     // bool SetScanTime(unsigned char scantime)
//     //    printf("\n>>>>>TESTING CODE FOR SET_SCAN_TIME<<<<<\n");
//     //    SetScanTime((unsigned char) 300);
//     //    GetResult()

//     //=========================TESTING CODE FOR INVENTORY_TID=========================================================================
//     //   printf("\n>>>>>TESTING CODE FOR INVENTORY TID<<<<<\n");
//     //    vector<ScanData> v;
//     //    bool ok = InventoryTID(v);
//     //    printtags(2);

//     //=========================TESTING CODE FOR MASK Inventory=========================================================================
//     //   printf("\n>>>>>TESTING CODE FOR INVENTORY TID<<<<<\n");
//   // SetInventoryMask();
//     //   vector<ScanData> v ;
//     //   unsigned char maskdata[] = {0x33, 0x30};
//     //// SetFilter(int maskAdrInByte, int maskLenInByte, unsigned char *maskDataByte);
////     SetFilter(0, 2, (unsigned char*) maskdata);
//     //    InventoryB(v, true);

//     // Set496Bits(true);
//     // bool resolution = Is496Bits();
//     //    printf("Is 496 bit? = %d\n", resolution);

//     // = Inventory(false);

//     // #if 0
//     //     int n;
//     //     unsigned char mask[] = {0xBE, 0xED};
//     //     SetFilter(0, 2, mask);
//     //     for(int i=0;i<10;i++) {
//     //         n = Inventory(false);
//     //         if (n == 0) {
//     //             printf("No tags detected.\n");
//     //         } else {
//     //             printf("Tags DETECTED [%d].\n", n);
//     //             printtag(n);
//     //         }
//     //     }
//     // #endif // 0
//     /*
//         unsigned char curpass[] = {0,0,0,0};
//         Auth(curpass, sizeof(curpass)); // @suppress("Invalid arguments")
//         int nn = Inventory(false); // @suppress("Invalid arguments")
//         extern vector<ScanData> scan_;
//         for(int i=0;i<nn;i++) {
//             ScanData sd = scan_[i];
//             string hex = tohex(sd.data);
//             printf("Ant = %d, Len = %lu, EPC = %s, RSSI = %d, Count = %d\n",
//                    (int)sd.ant, sd.data.length(), tohex(sd.data).c_str(), (int)sd.RSSI, sd.count);
//             //unsigned char pass[] = {0,0,0,0};
//             //SetPassword((unsigned char*)sd.data.data(), sd.data.length(), pass, 4);
//         }
//     */
//     CloseComPort();

//     printf("FINISHED.\n");
//     printf("Press ENTER to exit....");
//     getchar();

//     return 0;
// }
