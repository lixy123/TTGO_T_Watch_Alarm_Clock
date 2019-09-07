/*
   功能:  节能闹钟（休眠节能版)
   硬件： TTGO_T_Watch (带MAX98357 I2S DAC的扩展板 用于播放mp3)
   软件: arduino 安装有ESP32,TTGO-T-WATCH相关的库文件
   安装:
   1.上传声音资源文件到SPIFFS
     A. 安装如下位置的arduino 插件  https://github.com/me-no-dev/arduino-esp32fs-plugin
     B 重启Arduino，打开本项目 执行 Tools->ESP32 Sketch Data Upload 把本程序目录Data目录的mp3文件传到 esp32的SPIFFS目录中
   2.Arduino 打开本程序，编译，烧入程序

   程序原理:
   T-WATCH(本质上是ESP32加一些外设)开机后有如下4步
   A.初始化设备.
   B.读取rtc时钟（假定时钟提前已校对好)
   C.如果时钟点在闹钟的设定时间，就响铃（播放MP3)
   D.计算下到达下一个整点时间需要的休眠数。
   E.设置定时器休眠方式，并进入休眠.
   F.当定时休眠时间到后，进入A

   //关于尝试睡眠的几种方式的介绍如下:
   //https://blog.csdn.net/liwei16611/article/details/81354498

   省电研究:
   1.TTGO_T_Watch 如果连接手机充电头usb供电，休眠时电流为8ma，但如果连接笔记本usb供电，休眠电流为4ma
    感觉很好奇，于是反复对比，发现如果将手机充电头usb线的两根数据线的D-与GND相连接，则下降到到4ma。
   2.TTGO的工程师写了一个休眠程序的demo，能达到休眠2ma,经过反复试验，发现节能代码是
      tft.writecommand(0x10); 在休眠前调用可省掉2ma。
   我用的网上淘的便宜的电流仪，10几块钱一个，精度不太准。休眠电流应在1.5-2.5之间.
   以上两步骤就直接降到了2ma的电流能力.

   理论上:
   1.180ma内置电池能运行3-4天 (180/24/2=3.7)
   2.普通的18650电池直接供电,一般电量在2200ma，大约能供电 45天左右 (2200/24/2=45.8)
   响铃时电流60ma左右，一次用时仅2-3秒，相对全天用电可忽略
*/

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

// Uncomment to enable printing out nice debug messages.
//#define DEBUG_MODE

#ifdef DEBUG_MODE
// Define where debug output will be printed.
#define DEBUG_PRINTER Serial
#define DEBUG_BEGIN(...) { DEBUG_PRINTER.begin(__VA_ARGS__); }
#define DEBUG_PRINTM(...) { DEBUG_PRINTER.print(millis()/1000); DEBUG_PRINTER.print(__VA_ARGS__); }
#define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
#define DEBUG_PRINTMLN(...) { DEBUG_PRINTER.print(millis()/1000); DEBUG_PRINTER.println(__VA_ARGS__); }
#define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#define DEBUG_FLUSH(...) { DEBUG_PRINTER.flush();}
#else
#define DEBUG_BEGIN(...) {}
#define DEBUG_PRINTM(...) {}
#define DEBUG_PRINT(...) {}
#define DEBUG_PRINTMLN(...) {}
#define DEBUG_PRINTLN(...) {}
#define DEBUG_FLUSH(...) {}
#endif

#include "board_def.h"
#include <WiFi.h>

#include <Wire.h>
#include "pcf8563.h"  //rtc库
#include "axp20x.h"   //电源库，用于打开MAX98357的引脚
#include <TFT_eSPI.h>

#include "SPIFFS.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include <rom/rtc.h>


AudioGeneratorMP3 *mp3_player;
AudioGeneratorWAV *wav_player;
AudioFileSourceSPIFFS *audiofile;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

PCF8563_Class rtc;
AXP20X_Class axp;
TFT_eSPI tft = TFT_eSPI();

int TIME_TO_SLEEP = 60; //下次唤醒间隔时间(秒）

//多少分钟唤醒检查闹钟一次,配置时间长会节省唤醒时的电流消耗，更省电
//如果配置成5,则只有整5的分钟数才会检查闹钟， 07:06 则不会响铃
const int short_time_segment = 10;

//闹钟时间
//  *.05 表示小时不限，分钟数为5，秒数0
// 07:05 表示07:05:00 时间响铃
String time_set = "*:30:1,*:00:1";  //指定分钟响铃
//String time_set = "07:10:1,07:30:1";  //指定小时分钟响铃
String time_set_list[10];  //10个闹钟时间点，够用了
String time_def[3];



String playfn = "/dingdong.mp3";

String GetDigits(int digits, int len)
{
  String ret = String(digits);
  int addzero = len - ret.length();
  for (int loop1 = 0; loop1 < addzero; loop1++)
    ret = "0" + ret;
  return ret;
}

String Get_rtc_string()
{
  RTC_Date rtc_date = rtc.getDateTime();

  String timestr = "";

  timestr = GetDigits(rtc_date.year, 4) + "-" + GetDigits(rtc_date.month, 2) + "-" + GetDigits(rtc_date.day, 2) + " " +
            GetDigits(rtc_date.hour, 2) + ":" + GetDigits(rtc_date.minute, 2) + ":" + GetDigits(rtc_date.second, 2) + "(week:" +
            String( rtc.getDayOfWeek(rtc_date.day, rtc_date.month, rtc_date.year)) + ")";
  return (timestr);
}


void init_speaker_i2s()
{
  DEBUG_PRINTLN("init_speak_i2s");
  SPIFFS.begin();
  //打开MAX98357 必须的引脚(必须)
  //axp.begin();
  axp.setLDO3Mode(1);
  axp.setPowerOutPut(AXP202_LDO3, true);

  //注意：如果i2s麦克风占用通道0, 此处必须用通道1， 不能用同一通道，否则冲突！
  out = new AudioOutputI2S(1);

  // bool SetPinout(int bclkPin, int wclkPin, int doutPin);
  //配置i2s引脚
  out->SetPinout(TWATCH_DAC_IIS_BCK, TWATCH_DAC_IIS_WS, TWATCH_DAC_IIS_DOUT);
  out->SetGain(0.3); //调节音量大小 外接无源扬声器声音很响亮
  mp3_player = new AudioGeneratorMP3();
  wav_player = new AudioGeneratorWAV();

  audiofile = new AudioFileSourceSPIFFS();
  id3 = new AudioFileSourceID3(audiofile);
}

void play(int cnt)
{
  DEBUG_PRINTLN("play...");

  //初始化放在setup浪费时间，放在此处就可以，因为随后就睡眠状态了，不会有内存问题
  init_speaker_i2s();

  for (int loop1 = 0; loop1 < cnt; loop1++)
  {
    playmusic();
    if (loop1<cnt-1)
      delay(1000);
  }
}



void playmusic()
{

  DEBUG_PRINTLN("begin playmusic " + playfn);

  String tmpfn ;
  tmpfn = playfn;

  audiofile->open(tmpfn.c_str());

  uint32_t all_starttime;
  uint32_t all_endtime;
  all_starttime = millis() / 1000;

  if ( playfn.endsWith(".mp3"))
  {
    mp3_player->begin(id3, out);
    DEBUG_PRINTLN("play mp3 start:" + playfn);
    while (true)
    {
      if (mp3_player->isRunning()) {
        if (!mp3_player->loop())
        {
          mp3_player->stop();
        }
      }
      else
        break;
    }

    DEBUG_PRINTLN("end playmusic mp3");
  }

  else if ( playfn.endsWith(".wav"))
  {

    wav_player->begin(id3, out);
    DEBUG_PRINTLN("play wav start:" + playfn);
    while (true)
    {
      if (wav_player->isRunning()) {
        if (!wav_player->loop())
          wav_player->stop();
      }
      else
        break;
    }

    DEBUG_PRINTLN("end playmusic wav");
  }
  else
    DEBUG_PRINTLN("file can not play!");
  all_endtime = millis() / 1000;
  audiofile->close();
  DEBUG_PRINTLN("play time=" + String(all_endtime - all_starttime) + "秒");
}


void setup(void)
{

  uint32_t starttime = 0;
  uint32_t stoptime = 0;
  starttime = millis() / 1000;

  //注意：Serial串口输出关闭可以省少许电流
  
  DEBUG_BEGIN(115200);
  WiFi.mode(WIFI_OFF);
  DEBUG_PRINTLN("==== ==== ====>");
  RESET_REASON wakeup_reason =  rtc_get_reset_reason(0);
  print_reset_reason(wakeup_reason);

  Wire.begin(SEN_SDA, SEN_SCL);
  rtc.begin();
  //DEBUG_PRINTLN("read date and time from RTC:");
  DEBUG_PRINTLN("LocalTime:" + Get_rtc_string());

  axp.begin();
  //1.读取当前时间
  RTC_Date now = rtc.getDateTime();
  tft.init();
  // play(1);

  //2.检查闹钟定义的时间是否到了 (最多10个时间定义)
  //如果到了就响铃

  splitString(time_set, ",", time_set_list, 10);
  for (int loop1 = 0; loop1 < 10; loop1++)
  {
    String time_one = time_set_list[loop1];
    if (time_one.length() > 0)
    {
      splitString(time_one, ":", time_def, 3);

      //任意时间
      if  (time_def[0] == "*")
      {
        if (abs( now.minute * 60 + now.second -  time_def[1].toInt() * 60) < 30 && time_def[2].toInt() > 0)
        {
          int cnt = time_def[2].toInt();
          DEBUG_PRINTLN("=====> * play at timer:" + time_def[0] + ":" + time_def[1]);
          play( cnt);

        }
        continue;
      }
      else
      {

        if ( abs(now.hour * 3600 + now.minute * 60 + now.second -  time_def[0].toInt() * 3600 - time_def[1].toInt() * 60) < 30   && time_def[2].toInt() > 0)
        {
          int cnt = time_def[2].toInt();

          DEBUG_PRINTLN("=====> play at timer:" + time_def[0] + ":" + time_def[1]);
          play( cnt);
        }

      }
    }
  }


  //3.计算本次需要休眠秒数
  //如果short_time_segment是1，表示每1分钟整唤醒一次,定义的闹钟时间可随意
  //                       5，表示每5分钟整唤醒一次,这时定义的闹钟时间要是5的倍数，否则不会定时响铃
  TIME_TO_SLEEP = (short_time_segment - now.minute % short_time_segment) * 60;
  TIME_TO_SLEEP = TIME_TO_SLEEP - now.second;
  //休眠时间过短，低于10秒直接视同0
  if (TIME_TO_SLEEP < 10)
    TIME_TO_SLEEP = 60 * short_time_segment;

  //4.进入休眠
  //关闭LED显示器 节省2ma电流
  tft.writecommand(0x10);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  stoptime = millis() / 1000;
  DEBUG_PRINTLN("wake 用时:" + String(stoptime - starttime) + "秒");
  DEBUG_PRINTLN("go sleep,wake after " + String(TIME_TO_SLEEP)  + "秒");
  //DEBUG_FLUSH();
  //delay(50);

  //axp.setPowerOutPut(0xFF,false);  //电源管理：节能输出 (加上此句后系统无法唤醒,此句对节能没太大作用)

  // ESP进入deepSleep状态
  //最大时间间隔为 4,294,967,295 µs 约合71分钟
  //休眠后，GPIP的高，低状态将失效，无法用GPIO控制开关
  esp_deep_sleep_start();
}


void print_reset_reason(RESET_REASON reason)
{
  switch ( reason)
  {
    case 1 : DEBUG_PRINTLN("POWERON_RESET"); break;         /**<1,  Vbat power on reset*/
    case 3 : DEBUG_PRINTLN ("SW_RESET"); break;              /**<3,  Software reset digital core*/
    case 4 : DEBUG_PRINTLN("OWDT_RESET"); break;            /**<4,  Legacy watch dog reset digital core*/
    case 5 : DEBUG_PRINTLN ("DEEPSLEEP_RESET"); break;       /**<5,  Deep Sleep reset digital core*/
    case 6 : DEBUG_PRINTLN ("SDIO_RESET"); break;            /**<6,  Reset by SLC module, reset digital core*/
    case 7 : DEBUG_PRINTLN("TG0WDT_SYS_RESET"); break;      /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : DEBUG_PRINTLN ("TG1WDT_SYS_RESET"); break;      /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : DEBUG_PRINTLN("RTCWDT_SYS_RESET"); break;      /**<9,  RTC Watch dog Reset digital core*/
    case 10 : DEBUG_PRINTLN("INTRUSION_RESET"); break;      /**<10, Instrusion tested to reset CPU*/
    case 11 : DEBUG_PRINTLN ("TGWDT_CPU_RESET"); break;     /**<11, Time Group reset CPU*/
    case 12 : DEBUG_PRINTLN ("SW_CPU_RESET"); break;         /**<12, Software reset CPU*/
    case 13 : DEBUG_PRINTLN ("RTCWDT_CPU_RESET"); break;     /**<13, RTC Watch dog Reset CPU*/
    case 14 : DEBUG_PRINTLN ("EXT_CPU_RESET"); break;        /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : DEBUG_PRINTLN ("RTCWDT_BROWN_OUT_RESET"); break; /**<15, Reset when the vdd voltage is not stable*/
    case 16 : DEBUG_PRINTLN ("RTCWDT_RTC_RESET"); break;     /**<16, RTC Watch dog reset digital core and rtc module*/
    default : DEBUG_PRINTLN ("NO_MEAN");
  }
}


void splitString(String message, String dot, String outmsg[], int len)
{
  int commaPosition, outindex = 0;
  for (int loop1 = 0; loop1 < len; loop1++)
    outmsg[loop1] = "";
  do {
    commaPosition = message.indexOf(dot);
    if (commaPosition != -1)
    {
      outmsg[outindex] = message.substring(0, commaPosition);
      outindex = outindex + 1;
      message = message.substring(commaPosition + 1, message.length());
    }
    if (outindex >= len) break;
  }
  while (commaPosition >= 0);

  if (outindex < len)
    outmsg[outindex] = message;
}

void loop() {

}
