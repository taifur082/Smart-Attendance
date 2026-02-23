#include <Arduino.h>
#include <J4210U.h>

#define UHF_READER_BAUD 57600
#define SERIAL_LOG_BAUD 115200

// GPIO pin for controlling servo/relay
#define CONTROL_GPIO 5  // Change this to your desired GPIO pin

// ESP32 WROOM: Use Serial2 for UHF Reader on custom pins
#define UART2 Serial2

J4210U uhf(&UART2, UHF_READER_BAUD);

// Define your authorized EPC tags here
// Add or remove EPCs as needed
const char* authorizedEPCs[] = {
  "E2801190200051187E26CB52",  // Example EPC 1
  "E2801190200051187E26CB53",  // Example EPC 2
  "E2801190200051187E26CB54",
  "E2002083980B01320390E6EB",  // Example EPC 3
  // Add more EPCs here
};

const int numAuthorizedEPCs = sizeof(authorizedEPCs) / sizeof(authorizedEPCs[0]);

// Function to check if EPC is in authorized list
bool isAuthorizedEPC(const char* scannedEPC)
{
  for (int i = 0; i < numAuthorizedEPCs; i++)
  {
    if (strcmp(scannedEPC, authorizedEPCs[i]) == 0)
    {
      return true;
    }
  }
  return false;
}

int printSettings()
{
  ReaderInfo ri;
  if (uhf.GetSettings(&ri))
  {
    uhf.printsettings(&ri);
    return 1;
  }
  else
  {
    Serial.println("Failed to get settings from reader");
    return 0;
  }
}

int setSettings()
{
  ReaderInfo ri;
  ri.ScanTime = 300;
  ri.BeepOn = 1;
  
  if (!uhf.SetSettings(&ri))
  {
    Serial.println("Failed to set settings.");
    return 0;
  }
  
  Serial.println("Settings updated successfully!");
  uhf.GetSettings(&ri);
  uhf.printsettings(&ri);
  return 1;
}

void scan(bool getTid)
{
  Serial.println("\n========== SCAN START ==========");
  
  // Perform inventory scan
  int n = uhf.Inventory(false);
  
  Serial.print("TAGS DETECTED: ");
  Serial.println(n);
  
  if (n == 0)
  {
    Serial.println("No tags found");
    digitalWrite(CONTROL_GPIO, HIGH);  // No tags - GPIO HIGH
    Serial.println("GPIO: HIGH (No tags)");
    Serial.println("========== SCAN END ==========\n");
    return;
  }
  
  bool matchFound = false;
  
  // Process each detected tag
  for (int i = 0; i < n; i++)
  {
    Serial.print("\n--- Tag #");
    Serial.print(i + 1);
    Serial.println(" ---");
    
    ScanData *sd = uhf.GetResult(i);
    
    if (sd == NULL)
    {
      Serial.println("ERROR: Could not get scan data");
      break;
    }
    
    // Convert EPC to hex string
    char epcStr[64] = {0};
    
    if (sd->epclen > 0 && sd->epclen <= 32)
    {
      bytes2hex(sd->EPCNUM, sd->epclen, epcStr);
      
      Serial.print("EPC: ");
      Serial.println(epcStr);
      Serial.print("EPC Length: ");
      Serial.print(sd->epclen);
      Serial.println(" bytes");
      Serial.print("RSSI: ");
      Serial.println(sd->RSSI);
      
      // Check if this EPC is authorized
      if (isAuthorizedEPC(epcStr))
      {
        Serial.println("✓✓✓ AUTHORIZED TAG ✓✓✓");
        matchFound = true;
      }
      else
      {
        Serial.println("✗ Not in authorized list");
      }
    }
    else
    {
      Serial.print("ERROR: Invalid EPC length: ");
      Serial.println(sd->epclen);
    }
    
    // Optional: Get TID
    if (getTid && sd->epclen > 0)
    {
      unsigned char tid[16];
      unsigned char tidlen = 0;
      
      if (uhf.GetTID(sd->EPCNUM, sd->epclen, tid, &tidlen))
      {
        char tidstr[64];
        bytes2hex(tid, tidlen, tidstr);
        Serial.print("TID: ");
        Serial.println(tidstr);
      }
      else
      {
        Serial.println("Failed to get TID");
      }
    }
  }
  
  // Control GPIO based on match result
  Serial.println("\n--- GPIO CONTROL ---");
  if (matchFound)
  {
    digitalWrite(CONTROL_GPIO, LOW);   // Match found - GPIO LOW
    Serial.println("GPIO: LOW ✓ (Authorized tag detected)");
  }
  else
  {
    digitalWrite(CONTROL_GPIO, HIGH);  // No match - GPIO HIGH
    Serial.println("GPIO: HIGH ✗ (No authorized tags)");
  }
  
  Serial.println("========== SCAN END ==========\n");
}

void setup()
{
  // Setup GPIO for output
  pinMode(CONTROL_GPIO, OUTPUT);
  digitalWrite(CONTROL_GPIO, HIGH);  // Default state: HIGH (no match)
  
  // Serial for Serial Monitor (USB)
  Serial.begin(SERIAL_LOG_BAUD);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("ESP32 WROOM UHF Reader");
  Serial.println("with GPIO Control");
  Serial.println("=================================");
  
  // Print authorized EPCs
  Serial.println("\nAuthorized EPCs:");
  for (int i = 0; i < numAuthorizedEPCs; i++)
  {
    Serial.print("  [");
    Serial.print(i + 1);
    Serial.print("] ");
    Serial.println(authorizedEPCs[i]);
  }
  
  Serial.print("\nControl GPIO: Pin ");
  Serial.println(CONTROL_GPIO);
  Serial.println("  LOW = Authorized Tag Detected");
  Serial.println("  HIGH = No Authorized Tags");
  
  // Initialize Serial2 on pins 16(RX), 17(TX)
  Serial.println("\nInitializing Serial2 on pins 16(RX), 17(TX)...");
  Serial2.begin(UHF_READER_BAUD, SERIAL_8N1, 16, 17);
  
  delay(500);
  
  Serial.println("Configuring reader settings...");
  if (setSettings())
  {
    Serial.println("Reader configured successfully!");
  }
  else
  {
    Serial.println("WARNING: Could not configure reader.");
    Serial.println("Check connections:");
    Serial.println("  ESP32 Pin 16 (RX) -> UHF Reader TX");
    Serial.println("  ESP32 Pin 17 (TX) -> UHF Reader RX");
    Serial.println("  GND -> GND");
    Serial.println("  5V -> VCC");
  }
  
  Serial.println("\nCurrent reader settings:");
  printSettings();
  
  Serial.println("\n=================================");
  Serial.println("Starting scan loop...");
  Serial.println("Scan interval: 3 seconds");
  Serial.println("=================================\n");
  
  delay(2000);
}

void loop()
{
  scan(false);  // Set to true if you want TID
  delay(3000);   // Wait 3 seconds between scans
}