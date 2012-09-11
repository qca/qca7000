CFLAGS=-Wall
LDFLAGS=
CC=gcc

pl16cfg: pl16cfg.o

pl16cfg.o: pl16cfg.c

.c.o:
	${CC} -c ${<} ${CFLAGS}
.o:
	${CC} -o ${@} ${<} ${LDFLAGS}

install: pl16cfg
	mkdir -p "${DESTDIR}/root"
	cp pl16cfg "${DESTDIR}/root"

clean:
	rm -f pl16cfg *.core *.o
