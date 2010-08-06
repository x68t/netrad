#!/usr/bin/env python

import cgi
import time
import netrad
import config

f = cgi.FieldStorage()
cmd = f.getfirst('command')
arg = f.getfirst('arg')


def cmd_wait(fn, arg, status):
    fn(arg)
    time.sleep(4)
    for i in range(5):
        s = status()
        if s['result'] == '+OK':
            return s
        time.sleep(1)
    return dict(result='-ERR')


def list(arg):
    f = open('list.txt')
    return f.read()


netrad = netrad.netrad(config.node, config.service)
cmds = {
    'tune': lambda arg: cmd_wait(netrad.tune, arg, netrad.status),
    'stop': lambda arg: cmd_wait(netrad.stop, arg, netrad.status),
    'list': list,
    'status': netrad.status,
}
def nop(netrad, *arg):
    return dict(result='-ERR')

status = cmds.get(cmd, nop)(arg)
netrad.close()

status = str(status) + '\r\n'
print 'Content-Type: text/plain; charset=utf-8'
print 'Content-Length: %d' % len(status)
print ''
print status,
