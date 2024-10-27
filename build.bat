@echo off

if not exist build mkdir build
pushd build

cl -nologo -Od -Zi ../main.cpp

popd