#!/bin/sh
cd ext/cpputest
rm -rf cpputest_build/*
cd cpputest_build
cmake -DCMAKE_INSTALL_PREFIX=.. -DCOVERAGE=ON -DMEMORY_LEAK_DETECTION=ON ..
make 
make install
cd ../../..

