# ADQuake
by Crow_Bar <br>
modified by st1x51 <br>
Engine is still in test and may have bugs,which you dont like.  <br>

How to compile:<br>
# 1)Download and install pspsdk.<br>
https://sourceforge.net/projects/minpspw/ <br>
or <br>
https://darksectordds.github.io/html/MinimalistPSPSDK/index.html <br>
# 2)type make -f makefile install or run ADBuild.bat - for compiling Quake compatible engine <br>
type make -f Makefile ADQ_CUSTOM=1 install or run ADBuildStandalone.bat - if you want compile Custom engine with some features for Half-Life models,rendermodes <br>
if you have a error like this: <br>
(.text+0x58): undefined reference to isfinitef <br>
put libm.a in C:\pspsdk\lib\gcc\psp\8.2.0 <br>

for compiling with older compilers you can just use MakefileOld <br>
# 3)TODO <br>


