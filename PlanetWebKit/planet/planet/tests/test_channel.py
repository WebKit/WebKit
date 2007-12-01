#!/usr/bin/env python

import unittest
import planet
import tempfile
import ConfigParser

class FakePlanet:
    """
    A dummy Planet object that's enough to fool the
    Channel.__init__ method
    """

    def __init__(self):
        self.cache_directory = tempfile.gettempdir()
        self.config = ConfigParser.ConfigParser()

class FeedInformationTest(unittest.TestCase):
    """
    Test the Channel.feed_information method
    """

    def setUp(self):
        self.url = 'URL'
        self.changed_url = 'Changed URL'
        self.channel = planet.Channel(FakePlanet(), self.url)

    def test_unchangedurl(self):
        self.assertEqual(self.channel.feed_information(), '<%s>' % self.url)
               
    def test_changedurl(self):
        # change the URL directly
        self.channel.url = self.changed_url
        self.assertEqual(self.channel.feed_information(),
           "<%s> (formerly <%s>)" % (self.changed_url, self.url))

if __name__ == '__main__':
    unittest.main()
