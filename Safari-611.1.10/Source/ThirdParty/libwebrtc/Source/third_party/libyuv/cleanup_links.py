#!/usr/bin/env python
# Copyright 2017 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This is a copy of the file from WebRTC in:
# https://chromium.googlesource.com/external/webrtc/+/master/cleanup_links.py

"""Script to cleanup symlinks created from setup_links.py.

Before 177567c518b121731e507e9b9c4049c4dc96e4c8 (#15754) we had a Chromium
checkout which we created symlinks into. In order to do clean syncs after
landing that change, this script cleans up any old symlinks, avoiding annoying
manual cleanup needed in order to complete gclient sync.
"""

import logging
import optparse
import os
import shelve
import subprocess
import sys


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
LINKS_DB = 'links'

# Version management to make future upgrades/downgrades easier to support.
SCHEMA_VERSION = 1

class WebRTCLinkSetup(object):
  def __init__(self, links_db, dry_run=False):
    self._dry_run = dry_run
    self._links_db = links_db

  def CleanupLinks(self):
    logging.debug('CleanupLinks')
    for source, link_path  in self._links_db.iteritems():
      if source == 'SCHEMA_VERSION':
        continue
      if os.path.islink(link_path) or sys.platform.startswith('win'):
        # os.path.islink() always returns false on Windows
        # See http://bugs.python.org/issue13143.
        logging.debug('Removing link to %s at %s', source, link_path)
        if not self._dry_run:
          if os.path.exists(link_path):
            if sys.platform.startswith('win') and os.path.isdir(link_path):
              subprocess.check_call(['rmdir', '/q', '/s', link_path],
                                    shell=True)
            else:
              os.remove(link_path)
          del self._links_db[source]


def _initialize_database(filename):
  links_database = shelve.open(filename)
  # Wipe the database if this version of the script ends up looking at a
  # newer (future) version of the links db, just to be sure.
  version = links_database.get('SCHEMA_VERSION')
  if version and version != SCHEMA_VERSION:
    logging.info('Found database with schema version %s while this script only '
                 'supports %s. Wiping previous database contents.', version,
                 SCHEMA_VERSION)
    links_database.clear()
  links_database['SCHEMA_VERSION'] = SCHEMA_VERSION
  return links_database


def main():
  parser = optparse.OptionParser()
  parser.add_option('-d', '--dry-run', action='store_true', default=False,
                    help='Print what would be done, but don\'t perform any '
                         'operations. This will automatically set logging to '
                         'verbose.')
  parser.add_option('-v', '--verbose', action='store_const',
                    const=logging.DEBUG, default=logging.INFO,
                    help='Print verbose output for debugging.')
  options, _ = parser.parse_args()

  if options.dry_run:
    options.verbose = logging.DEBUG
  logging.basicConfig(format='%(message)s', level=options.verbose)

  # Work from the root directory of the checkout.
  script_dir = os.path.dirname(os.path.abspath(__file__))
  os.chdir(script_dir)

  # The database file gets .db appended on some platforms.
  db_filenames = [LINKS_DB, LINKS_DB + '.db']
  if any(os.path.isfile(f) for f in db_filenames):
    links_database = _initialize_database(LINKS_DB)
    try:
      symlink_creator = WebRTCLinkSetup(links_database, options.dry_run)
      symlink_creator.CleanupLinks()
    finally:
      for f in db_filenames:
        if os.path.isfile(f):
          os.remove(f)
  return 0


if __name__ == '__main__':
  sys.exit(main())
