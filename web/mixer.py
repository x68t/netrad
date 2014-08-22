#!/usr/bin/python

import alsaaudio


def getMixer():
    return alsaaudio.Mixer('PCM')


def getVolume():
    return getMixer().getvolume()[0]


def setVolume(volume):
    getMixer().setvolume(volume)
