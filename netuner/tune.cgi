#!/usr/bin/env python

import cgi
import time
import netrad
import config

f = cgi.FieldStorage()
url = f.getfirst('url')

netrad = netrad.netrad()
netrad.tune(url)
time.sleep(5)
status = netrad.status()

acc = []
keys = status.keys()
keys.sort()
for k in keys:
    acc.append('%s: %s' % (k, status[k]))
status = "\r\n".join(acc)

print 'Content-Type: text/plain; charset=utf-8'
print 'Content-Length: %d' % len(status)
print ''
print status,
