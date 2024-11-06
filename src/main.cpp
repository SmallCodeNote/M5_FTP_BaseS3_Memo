#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <time.h>
#include <M5_Ethernet.h>
#include <EEPROM.h>

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

void HTTPUI();

/// @brief Encorder Profile Struct
struct DATA_SET
{
  /// @brief IP address
  IPAddress ip;

  /// @brief UnitName
  String UnitName;
};
/// @brief Encorder Profile
DATA_SET data;

/// @brief Main Display
M5GFX Display_Main;
M5Canvas Display_Main_Canvas(&Display_Main);

void LoadEEPROM()
{
  EEPROM.begin(50); // 50byte
  EEPROM.get<DATA_SET>(0, data);
}

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
}

void draw_Title()
{
  M5.Display.setCursor(0, 0);
  M5.Display.println("Unit: " + data.UnitName + " ,ip: " + data.ip.toString());
}

String UnitName = "Unit";
String AddressString = "";
void setup()
{
  // Open serial communications and wait for port to open:
  M5Begin();

  LoadEEPROM();
  if (data.UnitName.length() > 0)
  {
    UnitName = data.UnitName;
    ip = data.ip;
    AddressString = ip.toString();
  }
  M5.Display.println(data.ip.toString());

  M5.Power.begin();
  EthernetBegin();
  server.begin();

  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  draw_Title();

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
  ftp.MakeDirRecursive("/" + UnitName + "/" + YYYY + "/" + YYYY + MM);
  ftp.AppendTextLine("/" + UnitName + "/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD + ".txt", timeLine);
  ftp.CloseConnection();

  HTTPUI();
  Ethernet.maintain();

  delay(1000);
}

void HTTPUI()
{
  EthernetClient client = server.available();
  if (client)
  {
    Serial.println("new client");
    boolean currentLineIsBlank = true;
    String currentLine = "";
    bool isPost = false;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank)
        {
          if (isPost)
          {
            // POSTデータの読み込み
            while (client.available())
            {
              char c = client.read();
              if (c == '\n' && currentLine.length() == 0)
              {
                break;
              }
              currentLine += c;
            }

            // POSTデータからUnitNameとIPaddressを抽出
            int startUnitName = currentLine.indexOf("unitname=") + 9;
            int endUnitName = currentLine.indexOf("&", startUnitName);
            if (endUnitName == -1)
            {
              endUnitName = currentLine.length();
            }
            UnitName = currentLine.substring(startUnitName, endUnitName);

            int startIPaddress = currentLine.indexOf("address=") + 8;
            int endIPaddress = currentLine.indexOf("&", startIPaddress);
            if (endIPaddress == -1)
            {
              endIPaddress = currentLine.length();
            }
            AddressString = currentLine.substring(startIPaddress, endIPaddress);

            Serial.println("UnitName: " + UnitName);
            Serial.println("IPaddress: " + AddressString);

            data.UnitName = UnitName;
            data.ip.fromString(AddressString);

            EEPROM.put<DATA_SET>(0, data);
            EEPROM.commit();
            delay(1000); // 1秒の遅延を追加 
            ESP.restart(); // M5Stackの再起動
          }

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<body>");
          client.println("<h1>M5Stack W5500 Test</h1>");
          client.println("<br />");
          client.print("UnitName: ");
          client.print(UnitName);
          client.println("<br />");
          client.print("IPaddress: ");
          client.print(AddressString);
          client.println("<br /><br />");
          client.println("<form action=\"/\" method=\"post\">");
          client.println("<ul>");
          client.println("<li>");
          client.println("<label for=\"unitname\">UnitName</label>");
          client.print("<input type=\"text\" id=\"unitname\" name=\"unitname\" value=\"");
          client.print(UnitName);
          client.println("\" required>");
          client.println("</li>");
          client.println("<li>");
          client.println("<label for=\"address\">IPaddress</label>");
          client.print("<input type=\"text\" id=\"address\" name=\"address\" value=\"");
          client.print(AddressString);
          client.println("\" required>");
          client.println("</li>");
          client.println("<li class=\"button\">");
          client.println("<button type=\"submit\">Save</button>");
          client.println("</li>");
          client.println("</ul>");
          client.println("</form>");
          client.println("</body>");
          client.println("</html>");
          break;
        }
        if (c == '\n')
        {
          currentLineIsBlank = true;
          currentLine = "";
        }
        else if (c != '\r')
        {
          currentLineIsBlank = false;
          currentLine += c;
        }
        if (currentLine.startsWith("POST /"))
        {
          isPost = true;
        }
      }
    }
    delay(1);
    client.stop();
    Serial.println("client disconnected");
  }
}
