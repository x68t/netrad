#!/usr/bin/python

import alsaaudio
import math


def getMixer():
    return alsaaudio.Mixer('PCM')


def getVolume():
    return math.pow(getMixer().getvolume()[0] / 100.0, 10)


def setVolume(volume):
    getMixer().setvolume(int(math.pow(float(volume), 1.0/10) * 100))


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
