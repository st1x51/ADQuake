# ADQuake
by Crow_Bar <br>
modified by st1x51 <br>
was created for custom games,not for quake(quake mods)<br>
Engine is still in development and may have bugs,which you dont like.  <br>

How to compile:<br>
# 1)Download and install pspsdk.<br>
https://sourceforge.net/projects/minpspw/ <br>
or <br>
https://darksectordds.github.io/html/MinimalistPSPSDK/index.html <br>
# 2)Compile Engine
type make -f makefile install or run ADBuild.bat - for compiling engine<br>
if you have a error like this: <br>
(.text+0x58): undefined reference to isfinitef <br>
put libm.a in C:\pspsdk\lib\gcc\psp\8.2.0 <br>
# 3)Write code in QC(progs.dat) or C(progs.prx)<br>


