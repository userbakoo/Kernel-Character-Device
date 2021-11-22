#i/bin/bash

rm -rf bld
mkdir bld
cd bld
cmake ..
make module
