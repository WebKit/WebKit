#!/usr/bin/env python
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Setup links to a Chromium checkout for WebRTC.

WebRTC standalone shares a lot of dependencies and build tools with Chromium.
To do this, many of the paths of a Chromium checkout is emulated by creating
symlinks to files and directories. This script handles the setup of symlinks to
achieve this.
"""


import ctypes
import errno
import logging
import optparse
import os
import shelve
import shutil
import subprocess
import sys
import textwrap


DIRECTORIES = [
  'build',
  'buildtools',
  'mojo',  # TODO(kjellander): Remove, see webrtc:5629.
  'native_client',
  'net',
  'testing',
  'third_party/binutils',
  'third_party/drmemory',
  'third_party/instrumented_libraries',
  'third_party/libjpeg',
  'third_party/libjpeg_turbo',
  'third_party/llvm-build',
  'third_party/lss',
  'third_party/proguard',
  'third_party/tcmalloc',
  'third_party/yasm',
  'third_party/WebKit',  # TODO(kjellander): Remove, see webrtc:5629.
  'tools/clang',
  'tools/gn',
  'tools/gyp',
  'tools/memory',
  'tools/python',
  'tools/swarming_client',
  'tools/valgrind',
  'tools/vim',
  'tools/win',
]

from sync_chromium import get_target_os_list
target_os = get_target_os_list()
if 'android' in target_os:
  DIRECTORIES += [
    'base',
    'third_party/accessibility_test_framework',
    'third_party/android_platform',
    'third_party/android_tools',
    'third_party/apache_velocity',
    'third_party/appurify-python',
    'third_party/ashmem',
    'third_party/bouncycastle',
    'third_party/catapult',
    'third_party/ced',
    'third_party/closure_compiler',
    'third_party/guava',
    'third_party/hamcrest',
    'third_party/icu',
    'third_party/icu4j',
    'third_party/ijar',
    'third_party/intellij',
    'third_party/jsr-305',
    'third_party/junit',
    'third_party/libxml',
    'third_party/mockito',
    'third_party/modp_b64',
    'third_party/ow2_asm',
    'third_party/protobuf',
    'third_party/requests',
    'third_party/robolectric',
    'third_party/sqlite4java',
    'third_party/zlib',
    'tools/android',
    'tools/grit',
  ]
if 'ios' in target_os:
  DIRECTORIES.append('third_party/class-dump')

FILES = {
  'tools/isolate_driver.py': None,
  'third_party/BUILD.gn': None,
}

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
CHROMIUM_CHECKOUT = os.path.join('chromium', 'src')
LINKS_DB = 'links'

# Version management to make future upgrades/downgrades easier to support.
SCHEMA_VERSION = 1


def query_yes_no(question, default=False):
  """Ask a yes/no question via raw_input() and return their answer.

  Modified from http://stackoverflow.com/a/3041990.
  """
  prompt = " [%s/%%s]: "
  prompt = prompt % ('Y' if default is True  else 'y')
  prompt = prompt % ('N' if default is False else 'n')

  if default is None:
    default = 'INVALID'

  while True:
    sys.stdout.write(question + prompt)
    choice = raw_input().lower()
    if choice == '' and default != 'INVALID':
      return default

    if 'yes'.startswith(choice):
      return True
    elif 'no'.startswith(choice):
      return False

    print "Please respond with 'yes' or 'no' (or 'y' or 'n')."


# Actions
class Action(object):
  def __init__(self, dangerous):
    self.dangerous = dangerous

  def announce(self, planning):
    """Log a description of this action.

    Args:
      planning - True iff we're in the planning stage, False if we're in the
                 doit stage.
    """
    pass

  def doit(self, links_db):
    """Execute the action, recording what we did to links_db, if necessary."""
    pass


class Remove(Action):
  def __init__(self, path, dangerous):
    super(Remove, self).__init__(dangerous)
    self._priority = 0
    self._path = path

  def announce(self, planning):
    log = logging.warn
    filesystem_type = 'file'
    if not self.dangerous:
      log = logging.info
      filesystem_type = 'link'
    if planning:
      log('Planning to remove %s: %s', filesystem_type, self._path)
    else:
      log('Removing %s: %s', filesystem_type, self._path)

  def doit(self, _):
    os.remove(self._path)


class Rmtree(Action):
  def __init__(self, path):
    super(Rmtree, self).__init__(dangerous=True)
    self._priority = 0
    self._path = path

  def announce(self, planning):
    if planning:
      logging.warn('Planning to remove directory: %s', self._path)
    else:
      logging.warn('Removing directory: %s', self._path)

  def doit(self, _):
    if sys.platform.startswith('win'):
      # shutil.rmtree() doesn't work on Windows if any of the directories are
      # read-only.
      subprocess.check_call(['rd', '/q', '/s', self._path], shell=True)
    else:
      shutil.rmtree(self._path)


class Makedirs(Action):
  def __init__(self, path):
    super(Makedirs, self).__init__(dangerous=False)
    self._priority = 1
    self._path = path

  def doit(self, _):
    try:
      os.makedirs(self._path)
    except OSError as e:
      if e.errno != errno.EEXIST:
        raise


class Symlink(Action):
  def __init__(self, source_path, link_path):
    super(Symlink, self).__init__(dangerous=False)
    self._priority = 2
    self._source_path = source_path
    self._link_path = link_path

  def announce(self, planning):
    if planning:
      logging.info(
          'Planning to create link from %s to %s', self._link_path,
          self._source_path)
    else:
      logging.debug(
          'Linking from %s to %s', self._link_path, self._source_path)

  def doit(self, links_db):
    # Files not in the root directory need relative path calculation.
    # On Windows, use absolute paths instead since NTFS doesn't seem to support
    # relative paths for symlinks.
    if sys.platform.startswith('win'):
      source_path = os.path.abspath(self._source_path)
    else:
      if os.path.dirname(self._link_path) != self._link_path:
        source_path = os.path.relpath(self._source_path,
                                      os.path.dirname(self._link_path))

    os.symlink(source_path, os.path.abspath(self._link_path))
    links_db[self._source_path] = self._link_path


class LinkError(IOError):
  """Failed to create a link."""
  pass


# Use junctions instead of symlinks on the Windows platform.
if sys.platform.startswith('win'):
  def symlink(source_path, link_path):
    if os.path.isdir(source_path):
      subprocess.check_call(['cmd.exe', '/c', 'mklink', '/J', link_path,
                             source_path])
    else:
      # Don't create symlinks to files on Windows, just copy the file instead
      # (there's no way to create a link without administrator's privileges).
      shutil.copy(source_path, link_path)
  os.symlink = symlink


class WebRTCLinkSetup(object):
  def __init__(self, links_db, force=False, dry_run=False, prompt=False):
    self._force = force
    self._dry_run = dry_run
    self._prompt = prompt
    self._links_db = links_db

  def CreateLinks(self, on_bot):
    logging.debug('CreateLinks')
    # First, make a plan of action
    actions = []

    for source_path, link_path in FILES.iteritems():
      actions += self._ActionForPath(
          source_path, link_path, check_fn=os.path.isfile, check_msg='files')
    for source_dir in DIRECTORIES:
      actions += self._ActionForPath(
          source_dir, None, check_fn=os.path.isdir,
          check_msg='directories')

    if not on_bot and self._force:
      # When making the manual switch from legacy SVN checkouts to the new
      # Git-based Chromium DEPS, the .gclient_entries file that contains cached
      # URLs for all DEPS entries must be removed to avoid future sync problems.
      entries_file = os.path.join(os.path.dirname(ROOT_DIR), '.gclient_entries')
      if os.path.exists(entries_file):
        actions.append(Remove(entries_file, dangerous=True))

    actions.sort()

    if self._dry_run:
      for action in actions:
        action.announce(planning=True)
      logging.info('Not doing anything because dry-run was specified.')
      sys.exit(0)

    if any(a.dangerous for a in actions):
      logging.warn('Dangerous actions:')
      for action in (a for a in actions if a.dangerous):
        action.announce(planning=True)
      print

      if not self._force:
        logging.error(textwrap.dedent("""\
        @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
                              A C T I O N     R E Q I R E D
        @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        Setting up the checkout requires creating symlinks to directories in the
        Chromium checkout inside chromium/src.
        To avoid disrupting developers, we've chosen to not delete directories
        forcibly, in case you have some work in progress in one of them :)

        ACTION REQUIRED:
        Before running `gclient sync|runhooks` again, you must run:
        %s%s --force

        Which will replace all directories which now must be symlinks, after
        prompting with a summary of the work-to-be-done.
        """), 'python ' if sys.platform.startswith('win') else '', __file__)
        sys.exit(1)
      elif self._prompt:
        if not query_yes_no('Would you like to perform the above plan?'):
          sys.exit(1)

    for action in actions:
      action.announce(planning=False)
      action.doit(self._links_db)

    if not on_bot and self._force:
      logging.info('Completed!\n\nNow run `gclient sync|runhooks` again to '
                   'let the remaining hooks (that probably were interrupted) '
                   'execute.')

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

  @staticmethod
  def _ActionForPath(source_path, link_path=None, check_fn=None,
                     check_msg=None):
    """Create zero or more Actions to link to a file or directory.

    This will be a symlink on POSIX platforms. On Windows it will result in:
    * a junction for directories
    * a copied file for single files.

    Args:
      source_path: Path relative to the Chromium checkout root.
        For readability, the path may contain slashes, which will
        automatically be converted to the right path delimiter on Windows.
      link_path: The location for the link to create. If omitted it will be the
        same path as source_path.
      check_fn: A function returning true if the type of filesystem object is
        correct for the attempted call. Otherwise an error message with
        check_msg will be printed.
      check_msg: String used to inform the user of an invalid attempt to create
        a file.
    Returns:
      A list of Action objects.
    """
    def fix_separators(path):
      if sys.platform.startswith('win'):
        return path.replace(os.altsep, os.sep)
      else:
        return path

    assert check_fn
    assert check_msg
    link_path = link_path or source_path
    link_path = fix_separators(link_path)

    source_path = fix_separators(source_path)
    source_path = os.path.join(CHROMIUM_CHECKOUT, source_path)
    if os.path.exists(source_path) and not check_fn:
      raise LinkError('Can only to link to %s: tried to link to: %s' %
                      (check_msg, source_path))

    if not os.path.exists(source_path):
      logging.debug('Silently ignoring missing source: %s. This is to avoid '
                    'errors on platform-specific dependencies.', source_path)
      return []

    actions = []

    if os.path.exists(link_path) or os.path.islink(link_path):
      if os.path.islink(link_path):
        actions.append(Remove(link_path, dangerous=False))
      elif os.path.isfile(link_path):
        actions.append(Remove(link_path, dangerous=True))
      elif os.path.isdir(link_path):
        actions.append(Rmtree(link_path))
      else:
        raise LinkError('Don\'t know how to plan: %s' % link_path)

    # Create parent directories to the target link if needed.
    target_parent_dirs = os.path.dirname(link_path)
    if (target_parent_dirs and
        target_parent_dirs != link_path and
        not os.path.exists(target_parent_dirs)):
      actions.append(Makedirs(target_parent_dirs))

    actions.append(Symlink(source_path, link_path))

    return actions

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
  on_bot = os.environ.get('CHROME_HEADLESS') == '1'

  parser = optparse.OptionParser()
  parser.add_option('-d', '--dry-run', action='store_true', default=False,
                    help='Print what would be done, but don\'t perform any '
                         'operations. This will automatically set logging to '
                         'verbose.')
  parser.add_option('-c', '--clean-only', action='store_true', default=False,
                    help='Only clean previously created links, don\'t create '
                         'new ones. This will automatically set logging to '
                         'verbose.')
  parser.add_option('-f', '--force', action='store_true', default=on_bot,
                    help='Force link creation. CAUTION: This deletes existing '
                         'folders and files in the locations where links are '
                         'about to be created.')
  parser.add_option('-n', '--no-prompt', action='store_false', dest='prompt',
                    default=(not on_bot),
                    help='Prompt if we\'re planning to do a dangerous action')
  parser.add_option('-v', '--verbose', action='store_const',
                    const=logging.DEBUG, default=logging.INFO,
                    help='Print verbose output for debugging.')
  options, _ = parser.parse_args()

  if options.dry_run or options.force or options.clean_only:
    options.verbose = logging.DEBUG
  logging.basicConfig(format='%(message)s', level=options.verbose)

  # Work from the root directory of the checkout.
  script_dir = os.path.dirname(os.path.abspath(__file__))
  os.chdir(script_dir)

  if sys.platform.startswith('win'):
    def is_admin():
      try:
        return os.getuid() == 0
      except AttributeError:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    if is_admin():
      logging.warning('WARNING: On Windows, you no longer need run as '
                      'administrator. Please run with user account privileges.')

  if not os.path.exists(CHROMIUM_CHECKOUT):
    logging.error('Cannot find a Chromium checkout at %s. Did you run "gclient '
                  'sync" before running this script?', CHROMIUM_CHECKOUT)
    return 2

  links_database = _initialize_database(LINKS_DB)
  try:
    symlink_creator = WebRTCLinkSetup(links_database, options.force,
                                      options.dry_run, options.prompt)
    symlink_creator.CleanupLinks()
    if not options.clean_only:
      symlink_creator.CreateLinks(on_bot)
  except LinkError as e:
    print >> sys.stderr, e.message
    return 3
  finally:
    links_database.close()
  return 0


if __name__ == '__main__':
  sys.exit(main())
