#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <time.h>
#include <M5_Ethernet.h>
// #include <EthernetUdp.h>

#include "M5_Ethernet_FtpClient.hpp"
#include "M5_Ethernet_NtpClient.hpp"

// == M5Basic_Bus ==
/*#define SCK  18
#define MISO 19
#define MOSI 23
#define CS   26
*/

// == M5CORES2_Bus ==
/*#define SCK  18
#define MISO 38
#define MOSI 23
#define CS   26
*/

// == M5PoECAM_Bus ==
/*#define SCK 23
#define MISO 38
#define MOSI 13
#define CS 4
*/

// == M5CORES3_Bus/M5CORES3_SE_Bus ==
#define SCK 36
#define MISO 35
#define MOSI 37
#define CS 9

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 25, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

EthernetClient FtpClient(21);

char ftp_server[] = "192.168.25.77";
char ftp_user[] = "ftpusr";
char ftp_pass[] = "ftpword";
char ftp_dirName[] = "/dataDir";
char ftp_newDirName[] = "/dataDir";
M5_Ethernet_FtpClient ftp(ftp_server, ftp_user, ftp_pass, 60000);

String timeServer = "192.168.25.77";

char textArray[] = "textArray";

void M5Begin()
{
  auto cfg = M5.config();
  cfg.serial_baudrate = 19200;
  M5.begin(cfg);
}
void EthernetBegin()
{
  SPI.begin(SCK, MISO, MOSI, -1);
  Ethernet.init(CS);
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
}

void setup()
{
  // Open serial communications and wait for port to open:
  M5Begin();
  M5.Power.begin();
  EthernetBegin();

  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  M5.Display.println("M5Stack W5500 Test");
  NtpClient.begin();
}

void loop()
{
  M5.Display.setCursor(0, 12);

  String timeLine = NtpClient.getTime(timeServer, +9);
  String YYYY = NtpClient.getYear();
  String MM = NtpClient.getMonth();
  String DD = NtpClient.getDay();
  String HH = NtpClient.getHour();

  M5.Display.println(timeLine);
  Serial.println(timeLine);

  ftp.OpenConnection();
  ftp.MakeDirRecursive("/S3/" + YYYY + "/" + YYYY + MM);
  ftp.AppendTextLine("/S3/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD + ".txt", timeLine);
  ftp.CloseConnection();

  Ethernet.maintain();

  delay(1000);
}
