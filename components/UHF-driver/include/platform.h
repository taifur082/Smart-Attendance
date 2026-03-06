#ifndef PLATFORM_H_INCLUDED
#define PLATFORM_H_INCLUDED

// #define WINDOWS
// #define MACOSX
// #define LINUX
#define ESP32

#ifdef WINDOWS
#include <windows.h>
#define DLLEXPORT __declspec(dllexport)

#endif // WINDOWS

#ifdef MACOSX
typedef unsigned char byte;
typedef int HANDLE;
#endif // MACOSX

#ifdef LINUX
typedef int HANDLE;
#include <memory.h>
#ifndef MACOSX
#define CLK_TCK CLOCK_TAI
#endif
#endif // LINUX

#include <vector>
#include <string>

#ifndef ESP32
#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus
  using namespace std;
  extern HANDLE platform_open(const char *reader_identifier, int baudrate);
  extern int platform_close(HANDLE h);
  extern int platform_write(HANDLE h, unsigned char *message_buffer, unsigned short buffer_size, unsigned short *number_bytes_transmitted);
  extern int platform_read(HANDLE h, unsigned char *message_buffer, unsigned short buffer_size, unsigned short *number_bytes_received, unsigned short timeout_ms);
  extern int platform_flush(HANDLE h);
  extern void platform_sleep(int milliseconds);
  extern vector<string> platform_list();
  extern void platform_clear(HANDLE h);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // ESP32

#ifdef ESP32
extern int platform_open(const char *reader_identifier, int baudrate);
extern int platform_close(int h);
extern int platform_write(int h, unsigned char *message_buffer, unsigned short buffer_size, unsigned short *number_bytes_transmitted);
extern int platform_read(int h, unsigned char *message_buffer, unsigned short buffer_size, unsigned short *number_bytes_received, unsigned short timeout_ms);
extern void platform_sleep(int milliseconds);

#endif // ESP32

#endif // PLATFORM_H_INCLUDED
