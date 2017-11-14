# Adjust these paths for your Posix version of MinGW.
W2XPOSIXMINGW=/usr/lib/gcc/i686-w64-mingw32/6.2-posix
W2XCOMMONMINGW=/usr/i686-w64-mingw32
CC=i686-w64-mingw32-g++-posix
STRIP=i686-w64-mingw32-strip
WINDRES=i686-w64-mingw32-windres
CXX_FLAGS= -static-libstdc++ -static-libgcc -static -pthread -fopenmp -march=i686 -Wall -std=c++11 -mno-avx2 -mno-avx -mno-sse4 -mno-ssse3 -msse3 -mfpmath=sse -O2 -fomit-frame-pointer -finline-functions
CXX_INCLUDES = -I../include -I$(W2XCOMMONMINGW)/include
LIBAFLAGS= -static-libstdc++ -static-libgcc -static $(W2XCOMMONMINGW)/lib/libm.a $(W2XPOSIXMINGW)/libgomp.a $(W2XCOMMONMINGW)/lib/libpthread.a $(W2XPOSIXMINGW)/libstdc++.a $(W2XPOSIXMINGW)/libgcc.a

all: zipper

zipper: zipper.o zipper_resource.o
	$(CC) -o $@.exe $^ $(LIBAFLAGS)
	$(STRIP) $@.exe

zipper_resource.o:
	$(WINDRES) --codepage=65001 "zipper.rc" -o "zipper_resource.o"

resources: zipper.o zipper_resource.o
	$(WINDRES) --codepage=65001 "zipper.rc" -o "zipper_resource.o"
	$(CC) -o zipper.exe $^ $(LIBAFLAGS)
	$(STRIP) zipper.exe

%.o: %.cpp
	$(CC) $(CXX_INCLUDES) -o $@ $(CXX_FLAGS) -c $+

clean:
	rm -f *.o zipper.exe
