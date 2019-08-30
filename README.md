
   用TTGO_T_Watch做的定时闹钟,缺设置闹钟功能,目标是用最少的电流实现闹钟功能.
   
   功能:  节能闹钟（休眠节能版), 为节能，放弃了显示屏功能.
         
   硬件： TTGO_T_Watch (带MAX98357 I2S DAC的扩展板)  
   
   软件: arduino 安装有ESP32,TTGO-T-WATCH相关的库文件
   
   安装:
   
   1.上传声音资源文件到SPIFFS
   
     A. 安装如下位置的arduino 插件  https://github.com/me-no-dev/arduino-esp32fs-plugin
     
     B 重启Arduino，打开本项目 执行 Tools->ESP32 Sketch Data Upload 把本程序目录Data目录的mp3文件传到 esp32的SPIFFS目录中
     
   2.Arduino 打开本程序，编译，烧入程序
     
   
   用电测试:
   
   A.如果用TTGO_T_Watch 内置的电池，理论睡眠电流是1-2ma, 180ma内置电池能运行3-4天 (未实际测试)
   
   B.如果用USB口直接供电,UART设备会消耗一些电流，加上esp32睡眠供电，实际是8ma (有实际测试)
   
     一个普通的18650电池直接供电,大约能供 10-12天左右,如果能跳过UART外电池供电，能减到2ma左右就好了
   
   响铃时电流60ma左右，因为仅仅2-3秒完成，相对全天用电可忽略。(有实际测试)
   
