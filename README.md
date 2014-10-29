# TPH Demo

This is a demo sketch for the SODAQ Mbili with a TPH board and a GPRSbee.

The main purpose of this sketch was to show the working of the TPH
board. The sensor values are collected and stored in the on-board dataflash
of the SODAQ Mbili. At certain intervals the data is set to an FTP server.

But there is a lot more in this sketch, such as
* the storage of configuration parameters in the EEPROM of the Atmega
* setting configuration parameters at startup
* timer that uses the RTC for scheduling long periods (RTCTimer)
* deep sleep to save battery power
* uploading data to an FTP server
* output of diagnostic messages to either a Serial of a Software Serial
  port

All the code is maintained in a GIT repository. One possible place to find
this GIT repo is at
[GitHub SodaqMoja](https://github.org/SodaqMoja/tph_demo.git)
The directory structure is such that it can be copied as is in your
Arduino IDE 1.5 sketchbook.

There is also a shell script (make-zip.sh) that can be used to create a ZIP
that contains everything. Here you can find the
[latest ZIP](http://downloads.sodaq.net/tph_demo.zip)
