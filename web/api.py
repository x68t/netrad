#!/usr/bin/python

from flask import Flask, Response, make_response, request
from json import dumps
from netrad import netrad
from db import session, Station


application = Flask(__name__)
application.debug = True

def tuneResponse(r):
    return Response(dumps(dict(status=r[0], body=r[1])), content_type='application/json')


def dictFactory(cursor, row):
    d = dict()
    for idx,col in enumerate(cursor.description):
        d[col[0]] = row[idx]
    return d


@application.route('/tune', methods=('PUT',))
def tune():
    json = request.json or dict()
    if type(json) == dict and json.has_key('url'):
        return tuneResponse(netrad().tune(request.json.get('url')))
    return make_response(dumps(json), 404)


@application.route('/status')
def status():
    return tuneResponse(netrad().status())


@application.route('/stop', methods=('PUT',))
def stop():
    return tuneResponse(netrad().stop())


@application.route('/station', methods=('GET',))
def getStations():
    return Response(dumps([i.dict() for i in session.query(Station).all()]), content_type='application/json')


@application.route('/station/<int:id>', methods=('GET',))
def getStation(id):
    return Response(dumps(session.query(Station).filter(Station.id==id).first().dict()), content_type='application/json')


@application.route('/station', methods=('POST',))
def postStation():
    json = request.json
    newStation = Station(json.get('site', ''), json.get('program', ''), json.get('genre', ''), json.get('url'))
    if newStation.url is None:
        return make_response('', 404)
    session.add(newStation)
    session.commit()
    return Response(dumps(newStation.dict()), content_type='application/json')


@application.route('/station/<int:id>', methods=('DELETE',))
def deleteStation(id):
    station = session.query(Station).filter(Station.id==id).first()
    if station is None:
        return make_response('', 404)
    session.delete(station)
    session.commit()
    return ''


@application.route('/station/<int:id>', methods=('PUT',))
def putStation(id):
    json = request.json
    station = session.query(Station).filter(Station.id==id).first()
    if station is None:
        return make_response('', 404)
    station.site = json.get('site', station.site)
    station.program = json.get('program', station.program)
    station.genre = json.get('genre', station.genre)
    station.url = json.get('url', station.url)
    session.commit()
    return Response(dumps(station.dict()), content_type='application/json')


@application.route('/volume', methods=('GET',))
def getVolume():
    import mixer
    return Response(dumps(mixer.getStatus()), content_type='application/json')


@application.route('/volume', methods=('PUT',))
def setVolume():
    import mixer
    #mixer.setStatus(request.get_json)
    mixer.setStatus(request.json)
    return ''


def cgiMain():
    from wsgiref.handlers import CGIHandler
    CGIHandler().run(application)


def wsgiMain():
    application.run(host='0.0.0.0', debug=True)


if __name__ == '__main__':
    #cgiMain()
    wsgiMain()
