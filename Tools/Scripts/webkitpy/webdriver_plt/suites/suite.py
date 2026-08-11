import json
import os


class Suite(object):
    def __init__(self, suitename):
        filename = "{0}.suite".format(suitename)
        filepath = os.path.join(os.path.dirname(__file__), filename)
        with open(filepath) as suitefile:
            _suite = json.load(suitefile)
        self.urls = _suite['urls']
        self.name = suitename
        self.attempts = 2
        self.max_attempts = 2

    @classmethod
    def get_available_suites(cls):
        suitesdir = os.path.dirname(__file__)
        suiteslist = [os.path.splitext(f)[0].lower() for f in os.listdir(suitesdir) if f.endswith('.suite')]
        if not suiteslist:
            raise Exception('Cant find any .suite files in directory %s' % suitesdir)
        return suiteslist
