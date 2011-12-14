#!/usr/bin/env python
import os, sys, shutil, errno, unittest
from ConfigParser import ConfigParser
from StringIO import StringIO
import planet

class MainTest(unittest.TestCase):
    
    def test_minimal(self):
        configp = ConfigParser()
        my_planet = planet.Planet(configp)
        my_planet.run("Planet Name", "http://example.com", [])

    def test_onefeed(self):
        configp = ConfigParser()
        configp.readfp(StringIO("""[http://www.example.com/]
name = Mary
"""))
        my_planet = planet.Planet(configp)
        my_planet.run("Planet Name", "http://example.com", [], True)


    def test_generateall(self):
        configp = ConfigParser()
        configp.readfp(StringIO("""[http://www.example.com/]
name = Mary
"""))
        my_planet = planet.Planet(configp)
        my_planet.run("Planet Name", "http://example.com", [], True)
        basedir = os.path.join(os.path.dirname(os.path.abspath(sys.modules[__name__].__file__)), 'data')
        os.mkdir(self.output_dir)
        t_file_names = ['simple', 'simple2']
        self._remove_cached_templates(basedir, t_file_names)
        t_files = [os.path.join(basedir, t_file) + '.tmpl' for t_file in t_file_names]
        my_planet.generate_all_files(t_files, "Planet Name",
                'http://example.com/', 'http://example.com/feed/', 'Mary', 'mary@example.com')
        for file_name in t_file_names:
            name = os.path.join(self.output_dir, file_name)
            content = file(name).read()
            self.assertEqual(content, 'Mary\n')

    def _remove_cached_templates(self, basedir, template_files):
        """
        Remove the .tmplc files and force them to be rebuilt.

        This is required mainly so that the tests don't fail in mysterious ways in
        directories that have been moved, eg 'branches/my-branch' to
        'branches/mysterious-branch' -- the .tmplc files seem to remember their full
        path
        """
        for file in template_files:
            path = os.path.join(basedir, file + '.tmplc')
            try:
                os.remove(path)
            except OSError, e:
                # we don't care about the file not being there, we care about
                # everything else
                if e.errno != errno.ENOENT:
                    raise

    def setUp(self):
        super(MainTest, self).setUp()
        self.output_dir = 'output'

    def tearDown(self):
        super(MainTest, self).tearDown()
        shutil.rmtree(self.output_dir, ignore_errors = True)
        shutil.rmtree('cache', ignore_errors = True)

if __name__ == '__main__':
    unittest.main()
