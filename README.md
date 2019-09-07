
   用TTGO_T_Watch做的定时闹钟,缺设置闹钟功能,目标是用最少的电流实现闹钟功能.
   
   功能:  节能闹钟（休眠节能版), 为节能，放弃了显示屏功能.
         
   硬件： TTGO_T_Watch (带MAX98357 I2S DAC的扩展板)  
   
   软件: arduino 安装有ESP32,TTGO-T-WATCH相关的库文件
   
   安装:
   
   1.上传声音资源文件到SPIFFS   
     A. 安装如下位置的arduino 插件  https://github.com/me-no-dev/arduino-esp32fs-plugin     
     B 重启Arduino，打开本项目 执行 Tools->ESP32 Sketch Data Upload 把本程序目录Data目录的mp3文件传到 esp32的SPIFFS目录中     
   2.Arduino 打开本程序，编译，烧入程序
     
   
   省电研究:<br/>
   如果要让 TTGO_T_Watch 体眠达到2ma, 必须同时满足如下2条件:<br/>
   A.如果用手机充电头usb供电，要将手机充电头usb线的两根数据线的D-数据线与GND相连接。 如果用电脑USB供电不用。<br/>
   B.进入休眠前调用 TFT_eSPI tft; tft.writecommand(0x10) 节能,此语句是关闭lcd供电 <br/>
   只要一个条件不满足，休眠时电流就是8ma,原理可能与usb供电机制有关系。

   1.180ma内置电池能运行3-4天 (180/24/2=3.7)<br/>
   2.普通的18650电池直接供电,一般电量在2200ma，大约能供电 45天左右 (2200/24/2=45.8)
   响铃时电流60ma左右，一次用时仅2-3秒，相对全天用电可忽略
   
   总的来说，可能是因为内置有节能硬件，T-WATCH的节能能力很强大。尝试过用esp32连接MAX98357+ds3231做过一个能mp3发声的闹钟，最小做到8ma就不错了.
   
   
   
