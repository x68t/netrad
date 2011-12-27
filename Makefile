EVENT_NOTIFIER = epoll
#EVENT_NOTIFIER = pselect

PREFIX = /usr/local
LIBEXEC = ${PREFIX}/libexec/netrad
CFLAGS = -O2 -march=native -Wall -pipe -DMP3_PLAYER='"${LIBEXEC}/player.libmpg123"' -DOGG_PLAYER='"${LIBEXEC}/player.libogg"' -DRAW_PLAYER='"${LIBEXEC}/player.raw"' -DNOISE_PLAYER='"${LIBEXEC}/player.noise"'

all: netrad player.libmpg123 player.libogg player.raw player.noise

netrad: netrad.o http.o audio.o event.o cdata.o client.o cmd.o logger.o growl.o libhhttpp/libhhttpp.a
	${CC} ${CFLAGS} -o $@ netrad.o http.o audio.o event.o cdata.o client.o cmd.o logger.o growl.o -Llibhhttpp -lhhttpp -lssl

player.libmpg123: player.libmpg123.o meta.o logger.o
	${CC} ${CFLAGS} -o $@ player.libmpg123.o meta.o logger.o -lmpg123 -lao

player.libogg: player.libogg.o
	${CC} ${CFLAGS} -o $@ player.libogg.o -lvorbisfile

player.raw: player.raw.o sndcard.o
	${CC} ${CFLAGS} -o $@ player.raw.o logger.o -lao

player.libmad: player.libmad.o
	${CC} ${CFLAGS} -o $@ player.libmad.o -lmad

player.noise: player.noise.o
	${CC} ${CFLAGS} -o $@ player.noise.o

event.o: event.${EVENT_NOTIFIER}.c
	${CC} ${CFLAGS} -c -o $@  event.${EVENT_NOTIFIER}.c

libhhttpp/libhhttpp.a:
	cd libhhttpp && make


install:: netrad player.libmpg123 player.libogg
	test -d ${PREFIX}/sbin || mkdir -p ${PREFIX}/sbin
	test -d ${LIBEXEC} || mkdir -p ${LIBEXEC}
	install -o root -g root netrad ${PREFIX}/sbin
	install -o root -g root player.libmpg123 player.libogg player.raw player.noise ${LIBEXEC}

start stop::
	/etc/init.d/netrad $@

clean::
	-rm -f *.o *~ netrad player.libmpg123 player.libogg player.raw player.noise
	cd libhhttpp && make clean

audio.o: audio.c audio.h event.h logger.h libhhttpp/hhttpp.h
cdata.o: cdata.c cdata.h
client.o: client.c event.h cmd.h client.h logger.h
cmd.o: cmd.c event.h audio.h http.h logger.h libhhttpp/hhttpp.h
event.epoll.o: event.epoll.c event.h cdata.h logger.h
http.o: http.c event.h http.h audio.h cmd.h logger.h libhhttpp/hhttpp.h
logger.o: logger.c logger.h
meta.o: meta.c meta.h logger.h
netrad.o: netrad.c event.h audio.h client.h logger.h
player.libmad.o: player.libmad.c
player.libmpg123.o: player.libmpg123.c meta.h logger.h
player.libogg.o: player.libogg.c
player.noise.o: player.noise.c
player.whitenoise.o: player.whitenoise.c
