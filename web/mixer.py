#!/usr/bin/python

import alsaaudio


def getMixer():
    return alsaaudio.Mixer('PCM')


def getVolume():
    return getMixer().getvolume()[0]


def setVolume(volume):
    getMixer().setvolume(int(volume))


def getMute():
    if getMixer().getmute()[0]:
        return True
    return False


def setMute(isMute):
    getMixer().setmute(isMute)


def getStatus():
    return dict(volume=getVolume(), mute=getMute())


def setStatus(status):
    volume = status.get('volume')
    if volume is not None:
        setVolume(volume)
    isMute = status.get('mute')
    if isMute is not None:
        setMute(isMute)
