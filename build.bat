@echo off

if not exist build mkdir build
pushd build

clang -g -O0 -Wall ../main.cpp -o main.exe -luser32.lib

popd