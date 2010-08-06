CFLAGS = -O2 -march=native -Wall

SRCS = buffer.c header.c hhttpp.c url.c
OBJS = $(SRCS:.c=.o)

all:: libhhttpp.a

a.out: z.c libhhttpp.a
	${CC} ${CFLAGS} -o $@ z.c libhhttpp.a

libhhttpp.a: $(OBJS)
	-rm $@
	ar q $@ $(OBJS)
	ranlib $@

z: libhhttpp.a z.c
	gcc ${CFLAGS} -o $@ z.c libhhttpp.a

valgrind:: z
	valgrind -v --leak-check=full --show-reachable=yes ./z

clean::
	rm -f *~ *.o *.gch *.a


buffer.o: buffer.c buffer.h hhttpp.h
header.o: header.c header.h
hhttpp.o: hhttpp.c hhttpp.h hhttpp-int.h url.h buffer.h header.h
url.o: url.c url.h
