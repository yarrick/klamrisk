CC=gcc

all: klamrisk

both: klamrisk klamrisk.exe

font.o: Allerta/allerta_medium.ttf
	objcopy --input binary --output elf32-i386 --binary-architecture i386 $^ $@

fontwin.o: Allerta/allerta_medium.ttf
	i686-mingw32-objcopy --input binary --output pe-i386 --binary-architecture i386 $^ $@

klamrisk.exe: klamrisk.c fontwin.o
	i686-mingw32-gcc $^ -lmingw32 -lSDLmain -lSDL -lopengl32 -lglu32 -lSDL_ttf -o $@ -DWIN32
	i686-mingw32-strip $@

klamrisk: klamrisk.c font.o
	gcc -o $@ $^ -lSDL -lGL -lGLU -lSDL_ttf
	strip $@

clean:
	rm -f klamrisk klamrisk-win32.exe font.o fontwin.o
