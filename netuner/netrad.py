#!/usr/bin/env python

import socket

def get_keyvalue(line):
    pos = line.find(':')
    if pos < 0:
        return None, None
    key = line[0:pos].strip().lower()
    value = line[pos+1:].strip()
    return key, value


def make_dict(lst):
    dic = {}
    for line in lst:
        key, value = get_keyvalue(line)
        if key:
            dic[key] = value
    return dic


class netrad:
    def __init__(self, node='localhost', service=9649):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect_ex((node, service))
        fd = sock.makefile('rw')
        self._sock = sock
        self._fd = fd

    def _command(self, cmd):
        fd = self._fd
        fd.write(cmd + '\n')
        fd.flush()
        r = []
        for line in fd:
            tok = line.split()[0]
            if tok in ('+OK', '-ERR'):
                return tok, r
            r.append(line.strip())
        return '-ERR', r

    def command(self, cmd):
        result, info = self._command(cmd)
        info.append('result: %s' % result)
        return make_dict(info)

    def close(self):
        self.command('close')
        self._fd.close()
        self._sock.close()

    def tune(self, URL):
        return self.command('tune %s' % URL)

    def stop(self, *arg):
        return self.command('stop')

    def status(self, *arg):
        return self.command('status')
