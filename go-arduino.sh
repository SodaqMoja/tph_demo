#!/bin/sh

MYDIR=$(dirname $0)
cd $MYDIR
[ ! -f preferences.txt ] && { echo "ERROR: Cannot find preferences.txt"; exit 1; }
sed -i "/^sketchbook.path=/s|=.*|=$(pwd)|" preferences.txt
#cp preferences.txt ~/.arduino15/

INO=$(echo */*.ino|head -1)
/usr/local/arduino-1.5.8/arduino --preferences-file ./preferences.txt $INO
