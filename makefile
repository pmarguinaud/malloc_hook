
CC = /home/mf/dp/marp/gco/apps/mkpack/support/wrapper/micc-18.0.5.274
FC = /home/mf/dp/marp/gco/apps/mkpack/support/wrapper/mimpifc-18.0.5.274 -shared-intel -heap-arrays 0 

main.x: main.F90 toto.F90 malloc_hook.o
	$(FC) -g -o main.x main.F90 toto.F90 malloc_hook.o

malloc_hook.o: malloc_hook.c
	$(CC) -g -c malloc_hook.c

clean:
	\rm -f *.o *.x
