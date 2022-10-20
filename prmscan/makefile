CF = -c -g -O2 -msmall-code -fbaserel -fno-builtin

prmscan: prmscan.o startup.o
	gcc -g -O2 -nostdlib -fbaserel -o PrmScan startup.o prmscan.o -lsmall

prmscan.o: prmscan.c
	gcc $(CF) $<

startup.o: startup.c
	gcc $(CF) -fwritable-strings $<

