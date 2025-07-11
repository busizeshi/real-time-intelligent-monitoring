#!/bin/bash
cd build
rm -rf ./*
cmake ..
make -j16

make install
cd ../install/
scp -r ./real_time_monitoring root@192.168.1.8:/demo/jwd_test/
