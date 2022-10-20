all: header.o driver.o
	gcc -s -nostdlib -msmall-code -o prm-rtl8029.device\
    header.o driver.o -lsmall

driver.o: driver.c ne2000.h rev.h
	gcc -O2 -c -msmall-code -fomit-frame-pointer driver.c

header.o: header.s rev.h
	gcc -c header.s

