echo off

goto(){
# Linux code here
uname -o
cd src
make -f unix.mak
}

goto $@
exit

:(){
rem Windows script here
echo %OS%
cd src
make 
exit
