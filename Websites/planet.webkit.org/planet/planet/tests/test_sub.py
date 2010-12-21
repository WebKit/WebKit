#!/usr/bin/env python
import os, glob, unittest
from ConfigParser import ConfigParser
from StringIO import StringIO
import planet

class SubTest(unittest.TestCase):
    
    def setUp(self):
        planet.logging.basicConfig()
        self.config = ConfigParser()
        self.config.add_section('Planet')
        self.config.set('Planet', 'cache_directory', 'planet/tests/data/cache')
        self.my_planet = planet.Planet(self.config)

    def tearDown(self):
        for file in glob.glob('planet/tests/data/cache/*'):
            os.unlink(file)
        os.removedirs('planet/tests/data/cache')

    def test_fetch(self):
        self.config.readfp(StringIO("""[planet/tests/data/before.atom]
name = Test Feed
"""))
        self.my_planet.run("test", "http://example.com", [], 0)
        channels, channels_list = self.my_planet.gather_channel_info()
        self.assertEqual(len(channels_list), 1)
        self.assertEqual(channels_list[0]['configured_url'],
            'planet/tests/data/before.atom')

        items_list = self.my_planet.gather_items_info(channels)
        self.assertEqual(len(items_list), 1)
        self.assertEqual(items_list[0]['summary'],'Some text.')
        self.assertEqual(items_list[0]['date_iso'],'2003-12-13T18:30:02+00:00')

    # this test is actually per the Atom spec definition of 'updated'
    def test_update_with_new_date(self):
        self.config.readfp(StringIO("""[planet/tests/data/before.atom]
name = Test Feed
"""))
        self.my_planet.run("test", "http://example.com", [], 0)
        channels, channels_list = self.my_planet.gather_channel_info()

        channel = channels.keys()[0]
        channel.url = 'planet/tests/data/after.atom'
        os.link('planet/tests/data/cache/planet,tests,data,before.atom',
                'planet/tests/data/cache/planet,tests,data,after.atom')
        channel.update()

        items_list = self.my_planet.gather_items_info(channels)
        self.assertEqual(len(items_list), 1)
        self.assertEqual(items_list[0]['summary'],'Updated text.')
        self.assertEqual(items_list[0]['date_iso'],'2006-05-21T18:54:02+00:00')

    def test_update_with_no_date(self):
        self.config.readfp(StringIO("""[planet/tests/data/before.rss]
name = Test Feed
"""))
        self.my_planet.run("test", "http://example.com", [], 0)

        channels, channels_list = self.my_planet.gather_channel_info()
        channel = channels.keys()[0]
        item=channel._items.values()[0]
        item.set_as_date('date',(2003, 12, 13, 18, 30, 2, 5, 347, 0))

        channel.url = 'planet/tests/data/after.rss'
        os.link('planet/tests/data/cache/planet,tests,data,before.rss',
                'planet/tests/data/cache/planet,tests,data,after.rss')
        items_list = self.my_planet.gather_items_info(channels)
        self.assertEqual(items_list[0]['date_iso'],'2003-12-13T18:30:02+00:00')
        channel.update()

        items_list = self.my_planet.gather_items_info(channels)
        self.assertEqual(len(items_list), 1)
        self.assertEqual(items_list[0]['summary'],'Updated text.')
        self.assertEqual(items_list[0]['date_iso'],'2003-12-13T18:30:02+00:00')

if __name__ == '__main__':
    unittest.main()
