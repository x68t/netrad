#!/usr/bin/python

from flask import Flask, Response, make_response, request
from json import dumps
from netrad import netrad


application = Flask(__name__)
application.debug = True

def response(r):
    return Response(dumps(dict(status=r[0], body=r[1])), content_type='application/javascript')


def dictFactory(cursor, row):
    d = dict()
    for idx,col in enumerate(cursor.description):
        d[col[0]] = row[idx]
    return d


@application.route('/tune/<path:url>', methods=('PUT',))
def tune(url):
    return response(netrad().tune(url))


@application.route('/status')
def status():
    return response(netrad().status())


@application.route('/stop', methods=('PUT',))
def stop():
    return response(netrad().stop())


@application.route('/stations')
def stations():
    from db import session, Station
    return make_response(dumps([i.dict() for i in session.query(Station).all()]))


@application.route('/volume', methods=('GET',))
def getVolume():
    import mixer
    return make_response(dumps(dict(volume=mixer.getVolume())))


@application.route('/volume', methods=('PUT',))
def setVolume():
    volume = request.json.get('volume')
    if volume is None:
        return make_response(dumps(request.json), 404)
    import mixer
    mixer.setVolume(volume)
    return ''


@application.route('/printenv')
def printenv():
    import os
    return make_response('\n'.join(['%s: %s' % (key, os.environ[key]) for key in os.environ]))


def cgiMain():
    from wsgiref.handlers import CGIHandler
    CGIHandler().run(application)


def wsgiMain():
    application.run(host='0.0.0.0', debug=True)


if __name__ == '__main__':
    cgiMain()
    #wsgiMain()
