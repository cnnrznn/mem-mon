BINS= libmm.so

libmm.so: dirtymon.c
	gcc dirtymon.c \
        -c -Wall -Werror -fpic \
        -shared -o libmm.so

clean:
	rm ${BINS}
