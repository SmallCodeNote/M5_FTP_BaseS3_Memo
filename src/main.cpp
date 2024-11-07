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

#define STORE_DATA_SIZE 64 // byte

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress deviceIP(192, 168, 25, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

EthernetClient FtpClient(21);

String ftp_address = "192.168.25.77";
String ftp_user = "ftpusr";
String ftp_pass = "ftpword";
String ftp_dirName = "/dataDir";
String ftp_newDirName = "/dataDir";

M5_Ethernet_FtpClient ftp(ftp_address, ftp_user, ftp_pass, 60000);

String ntp_address = "192.168.25.77";

char textArray[] = "textArray";

void HTTPUI();

/// @brief Encorder Profile Struct
struct DATA_SET
{
  /// @brief IP address
  IPAddress deviceIP;
  IPAddress ftpSrvIP;
  IPAddress ntpSrvIP;

  /// @brief deviceName
  String deviceName;
};
/// @brief Encorder Profile
DATA_SET storeData;

/// @brief Main Display
M5GFX Display_Main;
M5Canvas Display_Main_Canvas(&Display_Main);

void LoadEEPROM()
{
  EEPROM.begin(STORE_DATA_SIZE);
  EEPROM.get<DATA_SET>(0, storeData);
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
  Ethernet.begin(mac, deviceIP);
}

void draw_Title()
{
  M5.Display.setCursor(0, 0);
  M5.Display.println("Device: " + storeData.deviceName + " ,IP: " + storeData.deviceIP.toString());
}

String deviceName = "Device";
String deviceIP_String = "";
String ftpSrvIP_String = "";
String ntpSrvIP_String = "";

void setup()
{
  // Open serial communications and wait for port to open:
  M5Begin();
  LoadEEPROM();
  if (storeData.deviceName.length() > 0)
  {
    deviceName = storeData.deviceName;
    deviceIP = storeData.deviceIP;
    deviceIP_String = storeData.deviceIP.toString();
    ftpSrvIP_String = storeData.ftpSrvIP.toString();
    ntpSrvIP_String = storeData.ntpSrvIP.toString();
  }

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

  String timeLine = NtpClient.getTime(ntp_address, +9);
  String YYYY = NtpClient.readYear();
  String MM = NtpClient.readMonth();
  String DD = NtpClient.readDay();
  String HH = NtpClient.readHour();

  M5.Display.println(timeLine);
  Serial.println(timeLine);

  ftp.OpenConnection();
  ftp.MakeDirRecursive("/" + deviceName + "/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD);
  ftp.AppendTextLine("/" + deviceName + "/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD + "/" + YYYY + MM + DD + "_" + HH + ".txt", timeLine);
  ftp.CloseConnection();

  HTTPUI();
  Ethernet.maintain();

  delay(1000);
}

#define HTTP_GET_PARAM_FROM_POST(paramName)                                              \
  {                                                                                      \
    int start##paramName = currentLine.indexOf(#paramName "=") + strlen(#paramName "="); \
    int end##paramName = currentLine.indexOf("&", start##paramName);                     \
    if (end##paramName == -1)                                                            \
    {                                                                                    \
      end##paramName = currentLine.length();                                             \
    }                                                                                    \
    paramName = currentLine.substring(start##paramName, end##paramName);                 \
  }

#define HTML_PUT_INFOWITHLABEL(labelString) \
  client.print(#labelString ": ");          \
  client.print(labelString);                \
  client.println("<br />");

#define HTML_PUT_LI_INPUT(inputName)                                                             \
  {                                                                                              \
    client.println("<li>");                                                                      \
    client.println("<label for=\"" #inputName "\">" #inputName "</label>");                      \
    client.print("<input type=\"text\" id=\"" #inputName "\" name=\"" #inputName "\" value=\""); \
    client.print(inputName);                                                                     \
    client.println("\" required>");                                                              \
    client.println("</li>");                                                                     \
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
            // Load post data
            while (client.available())
            {
              char c = client.read();
              if (c == '\n' && currentLine.length() == 0)
              {
                break;
              }
              currentLine += c;
            }

            HTTP_GET_PARAM_FROM_POST(deviceName);
            HTTP_GET_PARAM_FROM_POST(deviceIP_String);
            HTTP_GET_PARAM_FROM_POST(ftpSrvIP_String);
            HTTP_GET_PARAM_FROM_POST(ntpSrvIP_String);

            Serial.println("deviceName: " + deviceName);
            Serial.println("IPaddress: " + deviceIP_String);

            storeData.deviceName = deviceName;
            storeData.deviceIP.fromString(deviceIP_String);
            storeData.ftpSrvIP.fromString(ftpSrvIP_String);
            storeData.ntpSrvIP.fromString(ntpSrvIP_String);

            EEPROM.put<DATA_SET>(0, storeData);
            EEPROM.commit();
            delay(1000);
            ESP.restart();
          }

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<body>");
          client.println("<h1>M5Stack W5500 Unit</h1>");
          client.println("<br />");

          /*
          HTML_PUT_INFOWITHLABEL(deviceName);
          HTML_PUT_INFOWITHLABEL(deviceIP_String);
          HTML_PUT_INFOWITHLABEL(ftpSrvIP_String);
          HTML_PUT_INFOWITHLABEL(ntpSrvIP_String);
          */

          client.println("<form action=\"/\" method=\"post\">");
          client.println("<ul>");

          HTML_PUT_LI_INPUT(deviceName);
          HTML_PUT_LI_INPUT(deviceIP_String);
          HTML_PUT_LI_INPUT(ftpSrvIP_String);
          HTML_PUT_LI_INPUT(ntpSrvIP_String);

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
