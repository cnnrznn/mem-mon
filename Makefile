BINS= libmm.so

libmm.so: dirtymon.c
	gcc dirtymon.c \
        -shared -Wall -fpic \
        -ldl \
        -o libmm.so

clean:
	rm ${BINS}
