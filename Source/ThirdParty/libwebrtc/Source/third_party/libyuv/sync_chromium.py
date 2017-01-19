#!/usr/bin/env python
# Copyright 2014 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script to download a Chromium checkout into the workspace.

The script downloads a full Chromium Git clone and its DEPS.

The following environment variable can be used to alter the behavior:
* CHROMIUM_NO_HISTORY - If set to 1, a Git checkout with no history will be
  downloaded. This is consumes less bandwidth and disk space but is known to be
  slower in general if you have a high-speed connection.

After a successful sync has completed, a .last_sync_chromium file is written to
the chromium directory. While it exists, no more gclient sync operations will be
performed until the --target-revision changes or the SCRIPT_VERSION constant is
incremented. The file can be removed manually to force a new sync.
"""

import argparse
import os
import subprocess
import sys

# Bump this whenever the algorithm changes and you need bots/devs to re-sync,
# ignoring the .last_sync_chromium file
SCRIPT_VERSION = 4

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
CHROMIUM_NO_HISTORY = 'CHROMIUM_NO_HISTORY'

def _parse_gclient_dict():
  gclient_dict = {}
  try:
    main_gclient = os.path.join(os.path.dirname(ROOT_DIR), '.gclient')
    with open(main_gclient, 'rb') as deps_content:
      exec(deps_content, gclient_dict)
  except Exception as e:
    print >> sys.stderr, 'error while parsing .gclient:', e
  return gclient_dict


def get_cache_dir():
  return _parse_gclient_dict().get('cache_dir')


def get_target_os_list():
  return ','.join(_parse_gclient_dict().get('target_os', []))


def main():
  CR_DIR = os.path.join(ROOT_DIR, 'chromium')

  p = argparse.ArgumentParser()
  p.add_argument('--target-revision', required=True,
                 help='The target chromium git revision [REQUIRED]')
  p.add_argument('--chromium-dir', default=CR_DIR,
                 help=('The path to the chromium directory to sync '
                       '(default: %(default)r)'))
  opts = p.parse_args()
  opts.chromium_dir = os.path.abspath(opts.chromium_dir)

  target_os_list = get_target_os_list()

  # Do a quick check to see if we were successful last time to make runhooks
  # sooper fast.
  flag_file = os.path.join(opts.chromium_dir, '.last_sync_chromium')
  flag_file_content = '\n'.join([
    str(SCRIPT_VERSION),
    opts.target_revision,
    repr(target_os_list),
  ])
  if (os.path.exists(os.path.join(opts.chromium_dir, 'src')) and
      os.path.exists(flag_file)):
    with open(flag_file, 'r') as f:
      if f.read() == flag_file_content:
        print 'Chromium already up to date: ', opts.target_revision
        return 0
    os.unlink(flag_file)

  env = os.environ.copy()

  # Avoid downloading NaCl toolchain as part of the Chromium hooks.
  env['GYP_CHROMIUM_NO_ACTION'] = '1'
  gclient_cmd = 'gclient.bat' if sys.platform.startswith('win') else 'gclient'
  args = [
      gclient_cmd, 'sync', '--force', '--revision', 'src@'+opts.target_revision
  ]

  if os.environ.get('CHROME_HEADLESS') == '1':
    # Running on a buildbot.
    args.append('-vvv')

    if sys.platform.startswith('win'):
      cache_path = os.path.join(os.path.splitdrive(ROOT_DIR)[0] + os.path.sep,
                                'b', 'git-cache')
    else:
      cache_path = '/b/git-cache'
  else:
    # Support developers setting the cache_dir in .gclient.
    cache_path = get_cache_dir()

  # Allow for users with poor internet connections to download a Git clone
  # without history (saves several gigs but is generally slower and doesn't work
  # with the Git cache).
  if os.environ.get(CHROMIUM_NO_HISTORY) == '1':
    if cache_path:
      print >> sys.stderr, (
          'You cannot use "no-history" mode for syncing Chrome (i.e. set the '
          '%s environment variable to 1) when you have cache_dir configured in '
          'your .gclient.' % CHROMIUM_NO_HISTORY)
      return 1
    args.append('--no-history')
    gclient_entries_file = os.path.join(opts.chromium_dir, '.gclient_entries')
  else:
    # Write a temporary .gclient file that has the cache_dir variable added.
    gclientfile = os.path.join(opts.chromium_dir, '.gclient')
    with open(gclientfile, 'rb') as spec:
      spec = spec.read().splitlines()
      spec[-1] = 'cache_dir = %r' % (cache_path,)
    with open(gclientfile + '.tmp', 'wb') as f:
      f.write('\n'.join(spec))

    args += [
      '--gclientfile', '.gclient.tmp',
      '--delete_unversioned_trees', '--reset', '--upstream'
    ]
    gclient_entries_file = os.path.join(opts.chromium_dir,
                                        '.gclient.tmp_entries')

  # To avoid gclient sync problems when DEPS entries have been removed we must
  # wipe the gclient's entries file that contains cached URLs for all DEPS.
  if os.path.exists(gclient_entries_file):
    os.unlink(gclient_entries_file)

  if target_os_list:
    args += ['--deps=' + target_os_list]

  print 'Running "%s" in %s' % (' '.join(args), opts.chromium_dir)
  ret = subprocess.call(args, cwd=opts.chromium_dir, env=env)
  if ret == 0:
    with open(flag_file, 'wb') as f:
      f.write(flag_file_content)

  return ret


if __name__ == '__main__':
  sys.exit(main())
