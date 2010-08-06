CFLAGS = -Wall -O0 -g -march=native `xml2-config --cflags`
LIBS = `xml2-config --libs` -lmemcached

all: netuner.cgi
netuner.cgi: netuner.cgi.o cache.o
	${CC} ${CFLAGS} -o $@ netuner.cgi.o cache.o ${LIBS}

install:: index.html netunes.css netunes.cgi netunes.js config.py netrad.py tune.cgi command.cgi
	cp -p index.html netunes.css netunes.cgi netunes.js config.py netrad.py tune.cgi command.cgi ${HOME}/Sites/home/netuner

clean::
	rm *.o *~
