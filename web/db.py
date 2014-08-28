#!/usr/bin/python

from sqlalchemy.ext.declarative import declarative_base, DeclarativeMeta
from sqlalchemy import Column, Integer, String
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
import json


Base = declarative_base()
class Station(Base):
    __tablename__ = 'stations'
    id = Column(Integer, primary_key=True)
    site = Column(String)
    program = Column(String)
    genre = Column(String)
    url = Column(String, unique=True)

    def __init__(self, site, program, genre, url):
        self.site = site
        self.program = program
        self.genre = genre
        self.url = url

    def __repr__(self):
        return '<Station %s>' % self.name

    def dict(self):
        return dict(id=self.id, site=self.site, program=self.program, genre=self.genre, url=self.url)


engine = create_engine('sqlite:///stations.sqlite3')
Session = sessionmaker(bind=engine)
session = Session()

if __name__ == '__main__':
    import os, grp
    try:
        os.unlink('stations.sqlite3')
    except:
        pass
    Base.metadata.create_all(engine)
    session.add_all([
            Station('181.fm', "Awesome 80's", "80's", 'www.181.fm/winamp.pls?station=181-awesome80s&file=181-awesome80s.pls'),
            Station('181.fm', "Lite 80's", "80's", 'www.181.fm/winamp.pls?station=181-lite80s&file=181-lite80s.pls'),
            Station('977music.com', "80's", "80's", 'www.977music.com/tunein/web/80s.asx')])
    session.commit()
    os.chown('stations.sqlite3', -1, grp.getgrnam('www-data').gr_gid)
    os.chmod('stations.sqlite3', 0664)
