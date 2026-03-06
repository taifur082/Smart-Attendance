#include <stdio.h>
/**
 * This is Hardware Abstraction Layer for serial communication on a PC's USB port.
 *
 */
#include "platform.h"
#include "common.h"

#include <time.h>
#include <sys/time.h>

using namespace std;
string error_;

#ifdef WINDOWS
#include <windows.h>

vector<string> platform_list() {
  vector<string> v;
  char lpTargetPath[ 5000 ]; // buffer to store the path of the COMPORTS
  bool gotPort = false;    // in case the port is not found
  char comport[ 16 ] = "";

  for (int i = 0; i < 255; i++) { // checking ports from COM0 to COM255
    snprintf(comport,sizeof(comport),"COM%d",i);
    DWORD test = QueryDosDevice(comport,lpTargetPath,5000);

    // Test the return value and error if any
    if (test) { // QueryDosDevice returns zero if it didn't find an object
      v.push_back(comport);
    }

    if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    }
  }
  return v;
}

void platform_sleep(int milliseconds) {
  Sleep(milliseconds);
  // Storing start time
  //    clock_t start_time = clock();

  // looping till required time is not achieved
  //    while (clock() < start_time + milliseconds)
  //        ;
}

bool platform_timeouts(HANDLE h,int readtimeout,int writetimeout) {
  // return true;
  COMMTIMEOUTS commtimeouts;
  int result = GetCommTimeouts(h,&commtimeouts);
  if (result) {
    if (readtimeout >= 0) {
      commtimeouts.ReadIntervalTimeout = 1;
      commtimeouts.ReadTotalTimeoutConstant = readtimeout;
      commtimeouts.ReadTotalTimeoutMultiplier = 1;
    }
    if (writetimeout >= 0) {
      commtimeouts.WriteTotalTimeoutConstant = 3;
      commtimeouts.WriteTotalTimeoutMultiplier = 2;
    }
    if (readtimeout >= 0 || writetimeout >= 0) {
      result = SetCommTimeouts(h,&commtimeouts);
      if (!result) {
        printf("Failed to set timeouts.\n");
        error_ = "Failed to set timeouts.";
      }
    }
    if (result)
      return true;
  }
  return false;
}

/* PLATFORM_OPEN_PORT_HANDLER */
HANDLE platform_open(const char* reader_identifier,int baudrate) {
  HANDLE h_port;
  DCB dcb;

  int result;
  char port_name[ 30 ];

  /* Map reader_identifier to COM port name */
#if defined(__GNUC__)
  strcpy(port_name,"\\\\.\\");
  strcat(port_name,(char*)reader_identifier);
  //    printf("Port Name: %s\n", port_name);
#else /* Visual Studio/Intel/etc */
  strcpy_s(port_name,sizeof(port_name),"\\\\.\\");
  strcat_s(port_name,sizeof(port_name),(char*)reader_identifier);
#endif

  /* Open serial port */
  h_port = CreateFileA(port_name,GENERIC_READ | GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

  /* Return error if invalid handle */
  if ((h_port == INVALID_HANDLE_VALUE)) {
    error_ = "Invalid Handle for port " + string(port_name) + ". Port may be in use or does not exist.";
    return 0;
  }

  /* Get default serial port parameters */
  dcb.DCBlength = sizeof(dcb);
  result = GetCommState(h_port,&dcb);

  /* Return error if unsuccessful getting serial port parameters */
  if (!result) {
    error_ = "Failed to set parameters for serial port : " + string(port_name);
    return 0;
  }

  /* Modify serial port parameters */
  dcb.BaudRate = baudrate;
  dcb.fParity = 0;
  dcb.Parity = NOPARITY;
  dcb.ByteSize = 8;
  dcb.StopBits = ONESTOPBIT;
  //    dcb.fOutX    = false;
  //    dcb.fInX     = false;
  //    dcb.fNull    = false;
  //    dcb.fDtrControl = DTR_CONTROL_DISABLE; //DTR_CONTROL_ENABLE;
  //    dcb.fRtsControl = RTS_CONTROL_DISABLE;

  /* Set serial port parameters */
  result = SetCommState(h_port,&dcb);

  /* Return error if unsuccessful setting serial port parameters */
  if (!result) {
    int e = GetLastError();
    char err[ 128 ];
    snprintf(err,sizeof(err),"Failed to set parameter for %s: Error# %d.",port_name,e);
    error_ = err;
    return 0;
  }

  /* Modify timeout values */
  result = platform_timeouts(h_port,1,1);

  if (!result) {
    int e = GetLastError();
    char err[ 128 ];
    snprintf(err,sizeof(err),"Failed to set timeout for %s: Error# %d.",port_name,e);
    error_ = err;
    return 0;
  }

  printf("Serial Port Connected: %s at baudrate %d\n",port_name,baudrate);
  /* Assign serial port handle to reader context */
  return h_port;
}

/* PLATFORM_CLOSE_PORT_HANDLER */
int platform_close(HANDLE h) {
  int result;

  /* Close reader handle */
  result = CloseHandle(h);

  if (!result) {
    return 0;
  }

  return 1;
}

/*
 * PLATFORM_TRANSMIT_HANDLER
 *
 * Return: 1: Success, 0:Fail
 */
int platform_write(HANDLE h,unsigned char* message_buffer,unsigned short buffer_size,
  unsigned short* number_bytes_transmitted) {
  int result;
  DWORD dw_bytes_written = 0;

  /* Write serial port */
  result = WriteFile(h,message_buffer,buffer_size,&dw_bytes_written,NULL);
  FlushFileBuffers(h);
  if (!result) {
    error_ = "Failed to write into device.";
    /* Fail */
    return 0;
  }
  else {
    /* Success */
    /* Assign number of bytes transmitted */
    *number_bytes_transmitted = (unsigned short)dw_bytes_written;
    return 1;
  }
}

/* PLATFORM_RECEIVE_HANDLER */
int platform_read(HANDLE h,unsigned char* message_buffer,unsigned short buffer_size,
  unsigned short* number_bytes_received,unsigned short timeout_ms) {
  int result;
  DWORD dw_bytes_received = 0;

  if (timeout_ms > 0) {
    result = platform_timeouts(h,timeout_ms,0);
    // Return if error if unsuccessful setting timeout parameters
    if (!result) {
      return 0;
    }
  }

  platform_timeouts(h,timeout_ms,0);
  /* Read serial port */
  result = ReadFile(h,message_buffer,buffer_size,&dw_bytes_received,NULL);
  if (result) {
    /* Assign bytes received */
    *number_bytes_received = (unsigned short)dw_bytes_received;
    if (dw_bytes_received == 0) {
      error_ = "The device returned no data.";
      return 0;
    }
    return 1;
  }
  error_ = "Failed to read from device.";
  return 0;
}

int platform_flush(HANDLE h) {
  int result;

  //    result = PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
  result = FlushFileBuffers(h);

  return result == 0 ? 1 : 0;
}

int platform_reset(HANDLE h,bool enable) {
  int action = enable ? SETDTR : CLRDTR;
  int result = EscapeCommFunction(h,action);
  return result == 0 ? 1 : 0;
}

int platform_wakeup(HANDLE h,bool enable) {
  int action = enable ? SETRTS : CLRRTS;
  int result = EscapeCommFunction(h,action);
  return result == 0 ? 1 : 0;
}

#endif

#ifdef LINUX

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <dirent.h>

static uint32_t baudrate_lookup(uint32_t baud) {
  switch (baud) {
  case 110:
    return B110;
  case 300:
    return B300;
  case 600:
    return B600;
  case 1200:
    return B1200;
  case 2400:
    return B2400;
  case 4800:
    return B4800;
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  case 230400:
    return B230400;
  }

  return 0;
}

HANDLE platform_open(const char* reader_identifier,int baudrate) {
  struct termios tio;
  int tty_fd;
  uint32_t baudrate_constant;

  /* Open serial port */
  char ri[ 256 ];
  if (strncmp(reader_identifier,"/dev/",5) != 0) {
    snprintf(ri,sizeof(ri),"/dev/%s",reader_identifier);
  }
  else {
    strcpy(ri,reader_identifier);
  }
  printf("Effective port: %s\n",ri);
  tty_fd = open((char*)ri,O_RDWR | O_NOCTTY | O_NDELAY); //works with mu903
  // tty_fd = open((char *)ri, O_RDWR | O_NOCTTY| O_NONBLOCK); //| O_NONBLOCK

  /* Return error if invalid handle */

  if (tty_fd < 0) {
    return 0;
  }

  fcntl(tty_fd,F_SETFL,0);

  baudrate_constant = baudrate_lookup(baudrate);

  tcgetattr(tty_fd,&tio);

  tio.c_iflag = IGNPAR; // INPCK;
  tio.c_lflag = 0;
  tio.c_oflag = 0;
  tio.c_cflag = CREAD | CS8 | CLOCAL;
  tio.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
  tio.c_cc[ VMIN ] = 0;
  tio.c_cc[ VTIME ] = 1;

  cfsetispeed(&tio,baudrate_constant);
  cfsetospeed(&tio,baudrate_constant);

  tcsetattr(tty_fd,TCSANOW,&tio);

  if (tty_fd > 0) {
    printf("Com port opened Successfully\n");
  }
  else {
    perror("Com port opening failed.");
    error_ = "Couldn't open com port.";
  }
  fflush(stdout);

  return (HANDLE)tty_fd;
}

/* PLATFORM_CLOSE_PORT_HANDLER */
int platform_close(HANDLE h) {
  if (close((intptr_t)h)) {
    return 0;
  }

  return 1;
}

void flushSerial(int fd) {
  // Flush both input and output buffers
  if (tcflush(fd,TCIOFLUSH) == -1) {
    perror("Error flushing serial port");
    // Handle the error, if needed
  }
}

/*
 * PLATFORM_TRANSMIT_HANDLER
 *
 * Return: 1: Success, 0:Fail
 */
int platform_write(
  HANDLE reader_context,
  uint8_t* message_buffer,
  uint16_t buffer_size,
  uint16_t* number_bytes_transmitted) {
  int bytes_written;
  flushSerial(reader_context);

  bytes_written = write(
    (intptr_t)reader_context,
    message_buffer,
    (intptr_t)buffer_size);

  if (bytes_written >= 0) {
    *number_bytes_transmitted = bytes_written;
    fflush(stdout);
    return 1;
  }
  else {
    perror("Writing failed. \n");
    error_ = "Writing failed.";
    fflush(stdout);
  }

  return 0;
}

/* PLATFORM_RECEIVE_HANDLER */
int platform_read(
  HANDLE reader_context,
  uint8_t* message_buffer,
  uint16_t buffer_size,
  uint16_t* number_bytes_received,
  uint16_t timeout_ms) {
  /* IRI's framer provides a per-transaction timeout, so we can
   * just operate in non-blocking mode and return nothing (provided
   * there are no errors) */
  int bytes_recvd;

  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;           // seconds
  timeout.tv_usec = (timeout_ms % 1000) * 1000; // microseconds

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET((intptr_t)reader_context,&readfds);

  int result = select((intptr_t)reader_context + 1,&readfds,NULL,NULL,&timeout);

  if (result > 0 && FD_ISSET((intptr_t)reader_context,&readfds)) {
    bytes_recvd = read((intptr_t)reader_context,message_buffer,buffer_size);

    if (bytes_recvd >= 0) {
      *number_bytes_received = bytes_recvd;
      return 1;
    }
  }

  return 0;
}

/* PLATFORM_TIMESTAMP_HANDLER */
int platform_timestamp_ms_handler() {
  struct timeval tv;

  gettimeofday(&tv,NULL);

  return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

void platform_sleep_ms_handler(uint32_t milliseconds) {
  usleep(milliseconds * 1000);
}

int platform_flush(HANDLE reader_context) {
  /* flush the port input */
  if (tcflush((intptr_t)reader_context,TCIOFLUSH)) {
    return 1;
  }

  return 0;
}

vector<string> platform_list() {
  vector<string> v;
  struct dirent* entry;
  DIR* dir = opendir("/dev/");
  if (dir == NULL) {
    return v;
  }
  while ((entry = readdir(dir)) != NULL) {
    std::string name = entry->d_name;
    std::string pref = name.substr(0,4);
    if (pref == "tty.") {
      v.push_back(entry->d_name);
    }
    else if (pref == "ttyU") {
      v.push_back(entry->d_name);
    }
    else if (pref == "ttyA") {
      v.push_back(entry->d_name);
    }
  }
  closedir(dir);

  return v;
}

void platform_sleep(int milliseconds) {
  usleep(milliseconds * 1000);
}

#endif // LINUX

#define ESP32
#ifdef ESP32

#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "sdkconfig.h"

#define LOG_TAG "core/src/hal-serial_local.cpp"
#define TXD_PIN ((gpio_num_t)CONFIG_UHF_UART_TXD_PIN)
#define RXD_PIN ((gpio_num_t)CONFIG_UHF_UART_RXD_PIN)

int platform_open(const char* reader_identifier,int baudrate) {
  esp_err_t ret;
  
  uart_config_t uart_config = {
    .baud_rate = baudrate,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT };

  ret = uart_param_config(UART_NUM_1,&uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
    return 0;
  }
  
  ret = uart_set_pin(UART_NUM_1,TXD_PIN,RXD_PIN,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
    return 0;
  }
  
  ret = uart_driver_install(UART_NUM_1,2048,0,0,NULL,0);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
    return 0;
  }

  ESP_LOGI(LOG_TAG, "UART%d initialized: TX=GPIO%d, RX=GPIO%d, Baud=%d", UART_NUM_1, TXD_PIN, RXD_PIN, baudrate);
  return UART_NUM_1;
}

int platform_close(int h) {
  uart_driver_delete(h);
  return true;
}

int platform_write(int h,
  unsigned char* message_buffer,
  unsigned short buffer_size,
  unsigned short* number_bytes_transmitted) {

  int bytes_written = uart_write_bytes(UART_NUM_1,message_buffer,buffer_size);
  if (bytes_written > 0) {
    *number_bytes_transmitted = bytes_written;
    return true;
  }

  return false;
}

int platform_read(int h,
  unsigned char* message_buffer,
  unsigned short buffer_size,
  unsigned short* number_bytes_received,
  unsigned short timeout_ms) {

  int bytes_read = uart_read_bytes(h,
    message_buffer,
    buffer_size,
    timeout_ms / portTICK_PERIOD_MS);

  if (bytes_read > 0)
  {
    *number_bytes_received = bytes_read;
    return true;
  }

  return false;
}


void platform_sleep(int time_ms) {
  vTaskDelay(time_ms / portTICK_PERIOD_MS);
}

#endif