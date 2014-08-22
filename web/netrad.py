#!/usr/bin/python

import socket
from contextlib import closing


class netrad:
    def __init__(self, host='localhost', port=9649):
        self.host = host
        self.port = port
        
    def _command(self, command):
        with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as sd:
            sd.connect_ex((self.host, self.port))
            with closing(sd.makefile('rw')) as fd:
                fd.write(command + '\n')
                fd.flush()
                body = []
                for line in fd:
                    tok = line.split()[0]
                    if tok in ('+OK', '-ERR'):
                        return tok, body
                    body.append(line.strip())
                return '-ERR', body

    def tune(self, url):
        r = self._command('TUNE %s' % url)
        r[1].insert(0, url);
        return r

    def stop(self):
        return self._command('STOP')

    def status(self):
        return self._command('STATUS')


if __name__ == '__main__':
    print netrad().tune("http://www.181.fm/winamp.pls?station=181-awesome80s&style=&description=Awesome%2080's")
