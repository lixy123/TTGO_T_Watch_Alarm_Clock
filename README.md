
   用TTGO_T_Watch做的定时闹钟,缺设置闹钟功能,目标是用最少的电流实现闹钟功能.
   
   功能:  节能闹钟（休眠节能版), 为节能，放弃了显示屏功能.
         
   硬件： TTGO_T_Watch (带MAX98357 I2S DAC的扩展板)  
   
   软件: arduino 安装有ESP32,TTGO-T-WATCH相关的库文件
   
   安装:
   
   1.上传声音资源文件到SPIFFS   
     A. 安装如下位置的arduino 插件  https://github.com/me-no-dev/arduino-esp32fs-plugin     
     B 重启Arduino，打开本项目 执行 Tools->ESP32 Sketch Data Upload 把本程序目录Data目录的mp3文件传到 esp32的SPIFFS目录中     
   2.Arduino 打开本程序，编译，烧入程序
     
   
   省电研究:
   
   1.TTGO_T_Watch 如果连接手机充电头usb供电，休眠时电流为8ma，但如果连接笔记本usb供电，休眠电流为4ma
    感觉很好奇，于是反复对比，发现如果将手机充电头usb线的两根数据线的D-与GND相连接，则下降到到4ma。
   2.TTGO的工程师写了一个休眠程序的demo，能达到休眠2ma,经过反复试验，发现节能代码是
      tft.writecommand(0x10); 在休眠前调用可省掉2ma。
   我用的网上淘的便宜的电流仪，10几块钱一个，精度不太准。休眠电流应在1.5-2.5之间.
   以上两步骤就直接降到了2ma的电流能力.

   1.180ma内置电池能运行3-4天 (180/24/2=3.7)
   2.普通的18650电池直接供电,一般电量在2200ma，大约能供电 45天左右 (2200/24/2=45.8)
   响铃时电流60ma左右，一次用时仅2-3秒，相对全天用电可忽略
   
