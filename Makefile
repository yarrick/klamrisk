CC=gcc

all: klamrisk

both: klamrisk klamrisk-win32.exe

klamrisk-win32.exe: klamrisk.c
	i686-mingw32-gcc $^ -lmingw32 -lSDLmain -lSDL -o $@ 
	i686-mingw32-strip $@

klamrisk: klamrisk.c
	$(CC) -o $@ $^ -lSDL
	strip $@

clean:
	rm -f klamrisk klamrisk-win32.exe
