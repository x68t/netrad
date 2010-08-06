#!/usr/bin/python
# -*- coding: iso-8859-1 -*-

import sys
import urllib
import urllib2
import cgi
import memcache
from BeautifulSoup import BeautifulSoup

menu_template = '<li><a href="netunes.cgi?menu_id=%(menu_id)s&title=%(name)s">%(name)s</a></li>'
station_template = '<li class="station" onclick="javascript:tune(this, \'%(url)s\');"><div class="bandwidth quality-%(quality)s">%(kbps)s</div><div class="name">%(station)s</div></li>'
head_template = 'Content-Type: text/plain; charset=utf-8\r\n\r\n<ul title="%(title)s (%(length)s)" id="menu_id-%(menu_id)s">'
tail_template = '</ul>'


def cache_open(menu_id):
    URL = 'http://pri.kts-af.net/xml/index.xml?sid=09A05772824906D82E3679D21CB1158B&tuning_id=%d' % menu_id
    mc = memcache.Client(['localhost:11211'], debug=0)
    xml = mc.get(URL);
    if not xml:
        opener = urllib2.build_opener()
        opener.addheaders = [('Accept', '*/*'),
                             ('User-Agent', 'iTunes/8.2 (Macintosh; N; Intel)'),
                             ('Accept-Language', 'en')]
        xml = opener.open(URL).read()
        mc.set(URL, xml, 7200)
    return BeautifulSoup(xml)


def menu_record(doc, title, menu_id):
    def p(record):
        try:
            name = record.find('kb:name').string
            menu_id = int(record.find('kb:menu_id').string)
            return menu_template % locals()
        except ValueError:
            return None

    records = []
    for record in doc.find('kb:menu_record').nextSiblingGenerator():
        if str(record).startswith('<'):
            r = p(record)
            if r:
                records.append(r)
    length = len(records)
    print head_template % locals()
    for i in records:
        print i
    print tail_template % locals()


def station_record(doc, title, menu_id):
    def p(record):
        try:
            station = record.find('kb:station').string.encode('iso-8859-1')
            url = urllib.quote(record.find('kb:station_url_record').find('kb:url').string.replace('&amp;', '&'))
            kbps = int(record.find('kb:station_url_record').find('kb:bandwidth_kbps').string)
            quality = 'high'
            if kbps < 56:
                quality = 'low'
            elif kbps < 128:
                quality = 'middle'
            return station_template % locals()
        except:
            return None

    records = []
    for record in doc.find('kb:station_record').nextSiblingGenerator():
        if str(record).startswith('<'):
            r = p(record)
            if r:
                records.append(r)
    length = len(records)
    print head_template % locals()
    for i in records:
        print i
    print tail_template % locals()


f = cgi.FieldStorage()
try:
    menu_id = int(f.getfirst('menu_id'))
    title = urllib.quote(f.getfirst('title') or 'Genre', safe=' /&\'')
except:
    print '<ul><li>invalid menu_id</li></ul>'
    sys.exit(0)

doc = cache_open(menu_id)
if menu_id == -12:
    menu_record(doc, title, menu_id)
else:
    station_record(doc, title, menu_id)
