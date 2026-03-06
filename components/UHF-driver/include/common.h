#ifndef PN532_H_INCLUDED
#define PN532_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>
#include <sys/time.h>
#include "driver.h"
#include "910driver.h"
#include "dtypes.h"
#define F(X) (X)

#ifdef WINDOWS
typedef unsigned char byte;
#endif

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
typedef int HANDLE;
#endif

extern int baud_;
using namespace std;
void toUpperCase(std::string str);
void getBytes(std::string str, unsigned char *buf, size_t len);
unsigned long millis();

extern "C" void printarr(unsigned char *, int size);
extern "C" string tohex(string &hex);                   // not found
extern "C" string bytes2hex(unsigned char *uid, int n); // not found
extern "C" string hex2bytes(string hex);                // not found
extern "C" double stopwatch(bool start);

extern void printsettings(ReaderInfo *ri);
extern void printarr(unsigned char *arr, int size);
extern double stopwatch(bool start);
extern void printtag(int n);
#endif // PN532_H_INCLUDED
