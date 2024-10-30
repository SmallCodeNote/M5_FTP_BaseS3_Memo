#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <time.h>
#include <M5_Ethernet.h>

#ifndef M5_Ethernet_NtpClient_H
#define M5_Ethernet_NtpClient_H

#define EthernetLinkUP_Retries 10 //

#define NTP_PACKET_SIZE 48 // NTP time stamp is in the first 48 bytes of the message
#define NTP_TASK_PRIO 1
#define NTP_TASK_STACKSIZE 4096
#define NTP_TZ "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"

class M5_Ethernet_NtpClient
{
    bool tcp_session = false;
    unsigned int localPort = 8888;

    byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
    char *timeServer = "ntp.jst.mfeed.ad.jp";
    const long gmtOffset_sec = 9 * 3600;
    const int daylightOffset_sec = 0;

    const int NTP_RETRY_NR = 4;            // #nr of retries
    const int NTP_RETRY_INTERVAL = 2000;   // time to wait between retries in ms
    const int NTP_UPDATE_INTERVAL = 10000; // Interval to start new update process

    EthernetUDP *Udp;
    bool EthernetUsable = false; // W5500 Usable flag

    RTC_DATA_ATTR int bootCount = 0;

    void begin(EthernetUDP *Udp);
    void printLocalTime();
    unsigned long sendNTPpacket(char *address);
    void syncTime(void *pvParameter);
    time_t elapsedTime, Previous_print_time;
};

void M5_Ethernet_NtpClient::begin(EthernetUDP *Udp)
{
    this->Udp = Udp;
    setenv("TZ", NTP_TZ, 1);
    tzset();
    printLocalTime();

    Udp->begin(123);
    /*
      if (EthernetUsable == true) {
        // Ethernet/W550 stack
        // Create task for NTP over Ethernet
        // https://techtutorialsx.com/2017/05/06/esp32-arduino-creating-a-task/
        xTaskCreate(&syncTime_Ethernet, "syncTime_Ethernet", NTP_TASK_STACKSIZE , NULL, NTP_TASK_PRIO, NULL);
      } else {
        // lwIP stack
        //void configTime (long gmtOffset_sec, int daylightOffset_sec, const char * server1, const char * server2, const char * server3)
        // https://www.esp8266.com/viewtopic.php?p=75120#p75120
        //configTime(0, 0, timeServer);
        configTzTime(NTP_TZ, timeServer);
      }
    */
}

// https://esp32.com/viewtopic.php?t=5398
// http://zetcode.com/articles/cdatetime/
// The strftime() function formats date and time.
// better use: size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
// https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.bpxbd00/strfti.htm

void M5_Ethernet_NtpClient::printLocalTime()
{
    struct tm timeinfo;
    char str_buf[1024];

    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    strftime(str_buf, sizeof(str_buf), "%A, %B %d %Y %H:%M:%S %F %T (%Z) weekday(sun): %u weeknumber(sun): %U weekday(mon): %w weeknumber(mon): %W TimeZoneOffset: %z ", &timeinfo);
    Serial.println(str_buf);

    // Code below seems limited by a 63 buffer, and prints garbled char when exceeding, do not use (code above just works fine)!!!
    /*
      // Serial has a default buffer of 64 chars, do not exceed...
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
      Serial.print(&timeinfo, "%F %T (%Z) ");
      Serial.print(&timeinfo, "weekday(sun): %u weeknumber(sun): %U " );
      Serial.print(&timeinfo, "weekday(mon): %w weeknumber(mon): %W ");
      Serial.println(&timeinfo, "TimeZoneOffset: %z Zone: (%Z)");

      Serial.print("Epoch:");
      Serial.println(mktime(&timeinfo));
    */
}

// send an NTP request to the time server at the given address
unsigned long M5_Ethernet_NtpClient::sendNTPpacket(char *address)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;          // Stratum, or type of clock
    packetBuffer[2] = 6;          // Polling Interval
    packetBuffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp->beginPacket(address, 123); // NTP requests are to port 123
    Udp->write(packetBuffer, NTP_PACKET_SIZE);
    Udp->endPacket();
}

void M5_Ethernet_NtpClient::syncTime(void *pvParameter)
{
    static bool This_NTP_Update_Was_Successfull = false;

    while (1)
    {
        int retry = 0;

        while ((This_NTP_Update_Was_Successfull == false) && retry++ < NTP_RETRY_NR)
        {
            sendNTPpacket(timeServer); // send an NTP packet to a time server
            vTaskDelay(1000 / portTICK_RATE_MS);

            if (Udp->parsePacket())
            {
                Udp->read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

                // the timestamp starts at byte 40 of the received packet and is four bytes,
                //  or two words, long. First, esxtract the two words:

                unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
                unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
                // combine the four bytes (two words) into a long integer
                // this is NTP time (seconds since Jan 1 1900):
                unsigned long secsSince1900 = highWord << 16 | lowWord;
                // Serial.print("Seconds since Jan 1 1900 = " );
                // Serial.println(secsSince1900);

                // now convert NTP time into everyday time:
                // Serial.print("Unix time = ");
                // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
                const unsigned long seventyYears = 2208988800UL;
                // subtract seventy years:
                // unsigned long epoch = secsSince1900 - seventyYears;
                time_t epoch = secsSince1900 - seventyYears;
                // print Unix time:
                // Serial.println(epoch);

                // print the hour, minute and second:
                // Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
                // Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
                // Serial.print(':');
                if (((epoch % 3600) / 60) < 10)
                {
                    // In the first 10 minutes of each hour, we'll want a leading '0'
                    // Serial.print('0');
                }
                // Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
                // Serial.print(':');
                if ((epoch % 60) < 10)
                {
                    // In the first 10 seconds of each minute, we'll want a leading '0'
                    // Serial.print('0');
                }
                // Serial.println(epoch % 60); // print the second

                {
                    struct tm tm;
                    char char_buf[1024];

                    // use _r restartable libary call's
                    localtime_r(&epoch, &tm);
                    time_t t = mktime(&tm);

                    // use _r restartable libary call's
                    // printf("Setting time: %s", asctime_r(&tm, char_buf));
                    // struct timeval now = { .tv_sec = t };
                    struct timeval now = {.tv_sec = epoch};

                    char str_buf[512];
                    strftime(str_buf, sizeof(str_buf), "%A, %B %d %Y %H:%M:%S %F (%Z) weekday(sun): %u weeknumber(sun): %U weekday(mon): %w weeknumber(mon): %W TimeZoneOffset: %z ", &tm);

                    // ESP_LOGI(TAG, "Time set to %s", str_buf);  // function to demo time

                    // code is writting in 2018, so year must be 2018 or greater, expecting not to go back in time any soon.
                    if (tm.tm_year >= (2018 - 1900))
                    {
                        // Returned time seems valid, lets set the time
                        settimeofday(&now, NULL);
                        ESP_LOGI(TAG, "Time set to %s", str_buf); // function to demo time
                        This_NTP_Update_Was_Successfull = true;
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Failed, invalid year %s", str_buf); // function to demo time
                    }
                }
            }
            if (This_NTP_Update_Was_Successfull == false)
                vTaskDelay(NTP_RETRY_INTERVAL / portTICK_RATE_MS); // retry timer
        }

        This_NTP_Update_Was_Successfull = false; // unset for next update interval
        // delay(5000);    // NTP update  interval
        //  This might not get reached when deep sleep is started before reaching this update interval.
        //  in this case we update always when wake-up/started, so it is not an issue.
        vTaskDelay(NTP_UPDATE_INTERVAL / portTICK_RATE_MS);
    }
}

#endif