#!/bin/bash
cd ~/development/real-time-intelligent-monitoring
cd build
rm -rf ./*
cmake ..
make -j16
cp ./real-time-intelligent-monitoring ../install/real_time_monitoring/bin
cd ../install/
scp -r ./real_time_monitoring root@192.168.1.8:/demo/jwd_test/
