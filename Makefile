CC=gcc

all: klamrisk

both: klamrisk klamrisk-win32.exe

klamrisk-win32.exe: klamrisk.c
	i686-mingw32-gcc $^ -lmingw32 -lSDLmain -lSDL -o $@ 

klamrisk: klamrisk.c
	$(CC) -o $@ $^ -lSDL

clean:
	rm -f klamrisk klamrisk-win32.exe
