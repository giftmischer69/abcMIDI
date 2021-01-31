echo off

goto(){
# Linux code here
uname -o
conda env create -f lin_env.yaml
chmod +x build.cmd
conda activate lin_abcMIDI
}

goto $@
exit

:(){
rem Windows script here
echo %OS%
conda env create -f win_env.yaml
call conda activate win_abcMIDI
exit
