# ADQuake
by Crow_Bar 
modified by st1x51
Engine is still in test and may have bugs,which you dont like.

How to compile:
1)Download and install pspsdk
https://sourceforge.net/projects/minpspw/
or
https://darksectordds.github.io/html/MinimalistPSPSDK/index.html
2)type make -f makefile install or run ADBuild.bat - for compiling Quake compatible engine
type make -f Makefile ADQ_CUSTOM=1 install or run ADBuildStandalone.bat - if you want compile Custom engine with some features for Half-Life models,rendermodes
if you have a error like this:
sf_logarithm.o): In function `logarithmf':
(.text+0x58): undefined reference to `isfinitef'
put libm.a in C:\pspsdk\lib\gcc\psp\8.2.0

for compiling with older compilers you can just use MakefileOld
3)TODO


