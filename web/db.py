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
    name = Column(String)
    url = Column(String, unique=True)

    def __init__(self, name, url):
        self.name = name
        self.url = url

    def __repr__(self):
        return '<Station %s>' % self.name

    def dict(self):
        return dict(id=self.id, name=self.name, url=self.url)


engine = create_engine('sqlite:///stations.sqlite3')
Session = sessionmaker(bind=engine)
session = Session()

if __name__ == '__main__':
    import os, grp
    os.unlink('stations.sqlite3')
    Base.metadata.create_all(engine)
    session.add_all([
            Station('181.fm', 'www.181.fm/winamp.pls?station=181-awesome80s&file=181-awesome80s.pls'),
            Station('1.fm', 'www.1.fm/TuneIn/WM/energy80s128k/Listen.aspx'),
            Station('977music.com', 'www.977music.com/tunein/web/80s.asx')])
    session.commit()
    os.chown('stations.sqlite3', -1, grp.getgrnam('www-data').gr_gid)
    os.chmod('stations.sqlite3', 0664)
