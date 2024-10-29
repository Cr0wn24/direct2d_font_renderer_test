@echo off

if not exist build mkdir build
pushd build

rem clang -g -O0 -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable ../dwrite_text_to_glyphs_example.cpp -o main.exe -luser32.lib
clang -g -O0 -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable ../d2d_text_rendering_example.cpp -o main.exe -luser32.lib

popd  