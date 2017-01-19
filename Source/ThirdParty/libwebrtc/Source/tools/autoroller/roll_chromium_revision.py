#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script to roll chromium_revision in the WebRTC DEPS file."""

import argparse
import base64
import collections
import logging
import os
import re
import subprocess
import sys
import urllib


CHROMIUM_SRC_URL = 'https://chromium.googlesource.com/chromium/src'
CHROMIUM_COMMIT_TEMPLATE = CHROMIUM_SRC_URL + '/+/%s'
CHROMIUM_LOG_TEMPLATE = CHROMIUM_SRC_URL + '/+log/%s'
CHROMIUM_FILE_TEMPLATE = CHROMIUM_SRC_URL + '/+/%s/%s'

COMMIT_POSITION_RE = re.compile('^Cr-Commit-Position: .*#([0-9]+).*$')
CLANG_REVISION_RE = re.compile(r'^CLANG_REVISION = \'(\d+)\'$')
ROLL_BRANCH_NAME = 'roll_chromium_revision'

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHECKOUT_ROOT_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, os.pardir,
                                                  os.pardir))
sys.path.append(CHECKOUT_ROOT_DIR)
import setup_links

sys.path.append(os.path.join(CHECKOUT_ROOT_DIR, 'build'))
import find_depot_tools
find_depot_tools.add_depot_tools_to_path()
from gclient import GClientKeywords

CLANG_UPDATE_SCRIPT_URL_PATH = 'tools/clang/scripts/update.py'
CLANG_UPDATE_SCRIPT_LOCAL_PATH = os.path.join('tools', 'clang', 'scripts',
                                              'update.py')

DepsEntry = collections.namedtuple('DepsEntry', 'path url revision')
ChangedDep = collections.namedtuple('ChangedDep',
                                    'path url current_rev new_rev')

class RollError(Exception):
  pass


def ParseDepsDict(deps_content):
  local_scope = {}
  var = GClientKeywords.VarImpl({}, local_scope)
  global_scope = {
    'From': GClientKeywords.FromImpl,
    'Var': var.Lookup,
    'deps_os': {},
  }
  exec(deps_content, global_scope, local_scope)
  return local_scope


def ParseLocalDepsFile(filename):
  with open(filename, 'rb') as f:
    deps_content = f.read()
  return ParseDepsDict(deps_content)


def ParseRemoteCrDepsFile(revision):
  deps_content = ReadRemoteCrFile('DEPS', revision)
  return ParseDepsDict(deps_content)


def ParseCommitPosition(commit_message):
  for line in reversed(commit_message.splitlines()):
    m = COMMIT_POSITION_RE.match(line.strip())
    if m:
      return m.group(1)
  logging.error('Failed to parse commit position id from:\n%s\n',
                commit_message)
  sys.exit(-1)


def _RunCommand(command, working_dir=None, ignore_exit_code=False,
                extra_env=None):
  """Runs a command and returns the output from that command.

  If the command fails (exit code != 0), the function will exit the process.

  Returns:
    A tuple containing the stdout and stderr outputs as strings.
  """
  working_dir = working_dir or CHECKOUT_ROOT_DIR
  logging.debug('CMD: %s CWD: %s', ' '.join(command), working_dir)
  env = os.environ.copy()
  if extra_env:
    assert all(type(value) == str for value in extra_env.values())
    logging.debug('extra env: %s', extra_env)
    env.update(extra_env)
  p = subprocess.Popen(command, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, env=env,
                       cwd=working_dir, universal_newlines=True)
  std_output = p.stdout.read()
  err_output = p.stderr.read()
  p.wait()
  p.stdout.close()
  p.stderr.close()
  if not ignore_exit_code and p.returncode != 0:
    logging.error('Command failed: %s\n'
                  'stdout:\n%s\n'
                  'stderr:\n%s\n', ' '.join(command), std_output, err_output)
    sys.exit(p.returncode)
  return std_output, err_output


def _GetBranches():
  """Returns a tuple of active,branches.

  The 'active' is the name of the currently active branch and 'branches' is a
  list of all branches.
  """
  lines = _RunCommand(['git', 'branch'])[0].split('\n')
  branches = []
  active = ''
  for line in lines:
    if '*' in line:
      # The assumption is that the first char will always be the '*'.
      active = line[1:].strip()
      branches.append(active)
    else:
      branch = line.strip()
      if branch:
        branches.append(branch)
  return active, branches


def _ReadGitilesContent(url):
  # Download and decode BASE64 content until
  # https://code.google.com/p/gitiles/issues/detail?id=7 is fixed.
  base64_content = ReadUrlContent(url + '?format=TEXT')
  return base64.b64decode(base64_content[0])


def ReadRemoteCrFile(path_below_src, revision):
  """Reads a remote Chromium file of a specific revision. Returns a string."""
  return _ReadGitilesContent(CHROMIUM_FILE_TEMPLATE % (revision,
                                                       path_below_src))


def ReadRemoteCrCommit(revision):
  """Reads a remote Chromium commit message. Returns a string."""
  return _ReadGitilesContent(CHROMIUM_COMMIT_TEMPLATE % revision)


def ReadUrlContent(url):
  """Connect to a remote host and read the contents. Returns a list of lines."""
  conn = urllib.urlopen(url)
  try:
    return conn.readlines()
  except IOError as e:
    logging.exception('Error connecting to %s. Error: %s', url, e)
    raise
  finally:
    conn.close()


def GetMatchingDepsEntries(depsentry_dict, dir_path):
  """Gets all deps entries matching the provided path.

  This list may contain more than one DepsEntry object.
  Example: dir_path='src/testing' would give results containing both
  'src/testing/gtest' and 'src/testing/gmock' deps entries for Chromium's DEPS.
  Example 2: dir_path='src/build' should return 'src/build' but not
  'src/buildtools'.

  Returns:
    A list of DepsEntry objects.
  """
  result = []
  for path, depsentry in depsentry_dict.iteritems():
    if path == dir_path:
      result.append(depsentry)
    else:
      parts = path.split('/')
      if all(part == parts[i]
             for i, part in enumerate(dir_path.split('/'))):
        result.append(depsentry)
  return result


def BuildDepsentryDict(deps_dict):
  """Builds a dict of DepsEntry object from a raw parsed deps dict."""
  result = {}
  def AddDepsEntries(deps_subdict):
    for path, deps_url in deps_subdict.iteritems():
      if not result.has_key(path):
        url, revision = deps_url.split('@') if deps_url else (None, None)
        result[path] = DepsEntry(path, url, revision)

  AddDepsEntries(deps_dict['deps'])
  for deps_os in ['win', 'mac', 'unix', 'android', 'ios', 'unix']:
    AddDepsEntries(deps_dict['deps_os'].get(deps_os, {}))
  return result


def CalculateChangedDeps(current_deps, new_deps):
  result = []
  current_entries = BuildDepsentryDict(current_deps)
  new_entries = BuildDepsentryDict(new_deps)

  all_deps_dirs = setup_links.DIRECTORIES
  for deps_dir in all_deps_dirs:
    # All deps have 'src' prepended to the path in the Chromium DEPS file.
    dir_path = 'src/%s' % deps_dir

    for entry in GetMatchingDepsEntries(current_entries, dir_path):
      new_matching_entries = GetMatchingDepsEntries(new_entries, entry.path)
      assert len(new_matching_entries) <= 1, (
          'Should never find more than one entry matching %s in %s, found %d' %
          (entry.path, new_entries, len(new_matching_entries)))
      if not new_matching_entries:
        result.append(ChangedDep(entry.path, entry.url, entry.revision, 'None'))
      elif entry != new_matching_entries[0]:
        result.append(ChangedDep(entry.path, entry.url, entry.revision,
                                 new_matching_entries[0].revision))
  return result


def CalculateChangedClang(new_cr_rev):
  def GetClangRev(lines):
    for line in lines:
      match = CLANG_REVISION_RE.match(line)
      if match:
        return match.group(1)
    raise RollError('Could not parse Clang revision!')

  chromium_src_path = os.path.join(CHECKOUT_ROOT_DIR, 'chromium', 'src',
                                   CLANG_UPDATE_SCRIPT_LOCAL_PATH)
  with open(chromium_src_path, 'rb') as f:
    current_lines = f.readlines()
  current_rev = GetClangRev(current_lines)

  new_clang_update_py = ReadRemoteCrFile(CLANG_UPDATE_SCRIPT_URL_PATH,
                                             new_cr_rev).splitlines()
  new_rev = GetClangRev(new_clang_update_py)
  return ChangedDep(CLANG_UPDATE_SCRIPT_LOCAL_PATH, None, current_rev, new_rev)


def GenerateCommitMessage(current_cr_rev, new_cr_rev, current_commit_pos,
                          new_commit_pos, changed_deps_list, clang_change):
  current_cr_rev = current_cr_rev[0:10]
  new_cr_rev = new_cr_rev[0:10]
  rev_interval = '%s..%s' % (current_cr_rev, new_cr_rev)
  git_number_interval = '%s:%s' % (current_commit_pos, new_commit_pos)

  commit_msg = ['Roll chromium_revision %s (%s)\n' % (rev_interval,
                                                    git_number_interval)]
  commit_msg.append('Change log: %s' % (CHROMIUM_LOG_TEMPLATE % rev_interval))
  commit_msg.append('Full diff: %s\n' % (CHROMIUM_COMMIT_TEMPLATE %
                                         rev_interval))
  # TBR field will be empty unless in some custom cases, where some engineers
  # are added.
  tbr_authors = ''
  if changed_deps_list:
    commit_msg.append('Changed dependencies:')

    for c in changed_deps_list:
      commit_msg.append('* %s: %s/+log/%s..%s' % (c.path, c.url,
                                                  c.current_rev[0:10],
                                                  c.new_rev[0:10]))
      if 'libvpx' in c.path:
        tbr_authors += 'marpan@webrtc.org, '

    change_url = CHROMIUM_FILE_TEMPLATE % (rev_interval, 'DEPS')
    commit_msg.append('DEPS diff: %s\n' % change_url)
  else:
    commit_msg.append('No dependencies changed.')

  if clang_change.current_rev != clang_change.new_rev:
    commit_msg.append('Clang version changed %s:%s' %
                      (clang_change.current_rev, clang_change.new_rev))
    change_url = CHROMIUM_FILE_TEMPLATE % (rev_interval,
                                           CLANG_UPDATE_SCRIPT_URL_PATH)
    commit_msg.append('Details: %s\n' % change_url)
  else:
    commit_msg.append('No update to Clang.\n')

  commit_msg.append('TBR=%s' % tbr_authors)
  commit_msg.append('BUG=None')
  return '\n'.join(commit_msg)


def UpdateDeps(deps_filename, old_cr_revision, new_cr_revision):
  """Update the DEPS file with the new revision."""
  with open(deps_filename, 'rb') as deps_file:
    deps_content = deps_file.read()
  deps_content = deps_content.replace(old_cr_revision, new_cr_revision)
  with open(deps_filename, 'wb') as deps_file:
    deps_file.write(deps_content)

def _IsTreeClean():
  stdout, _ = _RunCommand(['git', 'status', '--porcelain'])
  if len(stdout) == 0:
    return True

  logging.error('Dirty/unversioned files:\n%s', stdout)
  return False


def _EnsureUpdatedMasterBranch(dry_run):
  current_branch = _RunCommand(
      ['git', 'rev-parse', '--abbrev-ref', 'HEAD'])[0].splitlines()[0]
  if current_branch != 'master':
    logging.error('Please checkout the master branch and re-run this script.')
    if not dry_run:
      sys.exit(-1)

  logging.info('Updating master branch...')
  _RunCommand(['git', 'pull'])


def _CreateRollBranch(dry_run):
  logging.info('Creating roll branch: %s', ROLL_BRANCH_NAME)
  if not dry_run:
    _RunCommand(['git', 'checkout', '-b', ROLL_BRANCH_NAME])


def _RemovePreviousRollBranch(dry_run):
  active_branch, branches = _GetBranches()
  if active_branch == ROLL_BRANCH_NAME:
    active_branch = 'master'
  if ROLL_BRANCH_NAME in branches:
    logging.info('Removing previous roll branch (%s)', ROLL_BRANCH_NAME)
    if not dry_run:
      _RunCommand(['git', 'checkout', active_branch])
      _RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])


def _LocalCommit(commit_msg, dry_run):
  logging.info('Committing changes locally.')
  if not dry_run:
    _RunCommand(['git', 'add', '--update', '.'])
    _RunCommand(['git', 'commit', '-m', commit_msg])


def _UploadCL(dry_run, rietveld_email=None):
  logging.info('Uploading CL...')
  if not dry_run:
    cmd = ['git', 'cl', 'upload', '-f']
    if rietveld_email:
      cmd.append('--email=%s' % rietveld_email)
    _RunCommand(cmd, extra_env={'EDITOR': 'true'})


def _SendToCQ(dry_run, skip_cq):
  logging.info('Sending the CL to the CQ...')
  if not dry_run and not skip_cq:
    _RunCommand(['git', 'cl', 'set_commit'])
    logging.info('Sent the CL to the CQ.')


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--clean', action='store_true', default=False,
                 help='Removes any previous local roll branch.')
  p.add_argument('-r', '--revision',
                 help=('Chromium Git revision to roll to. Defaults to the '
                       'Chromium HEAD revision if omitted.'))
  p.add_argument('-u', '--rietveld-email',
                 help=('E-mail address to use for creating the CL at Rietveld'
                       'If omitted a previously cached one will be used or an '
                       'error will be thrown during upload.'))
  p.add_argument('--dry-run', action='store_true', default=False,
                 help=('Calculate changes and modify DEPS, but don\'t create '
                       'any local branch, commit, upload CL or send any '
                       'tryjobs.'))
  p.add_argument('--allow-reverse', action='store_true', default=False,
                 help=('Allow rolling back in time (disabled by default but '
                       'may be useful to be able do to manually).'))
  p.add_argument('--skip-cq', action='store_true', default=False,
                 help='Skip sending the CL to the CQ (default: %(default)s)')
  p.add_argument('-v', '--verbose', action='store_true', default=False,
                 help='Be extra verbose in printing of log messages.')
  opts = p.parse_args()

  if opts.verbose:
    logging.basicConfig(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.INFO)

  if not _IsTreeClean():
    logging.error('Please clean your local checkout first.')
    return 1

  if opts.clean:
    _RemovePreviousRollBranch(opts.dry_run)

  _EnsureUpdatedMasterBranch(opts.dry_run)

  new_cr_rev = opts.revision
  if not new_cr_rev:
    stdout, _ = _RunCommand(['git', 'ls-remote', CHROMIUM_SRC_URL, 'HEAD'])
    head_rev = stdout.strip().split('\t')[0]
    logging.info('No revision specified. Using HEAD: %s', head_rev)
    new_cr_rev = head_rev

  deps_filename = os.path.join(CHECKOUT_ROOT_DIR, 'DEPS')
  local_deps = ParseLocalDepsFile(deps_filename)
  current_cr_rev = local_deps['vars']['chromium_revision']

  current_commit_pos = ParseCommitPosition(ReadRemoteCrCommit(current_cr_rev))
  new_commit_pos = ParseCommitPosition(ReadRemoteCrCommit(new_cr_rev))

  current_cr_deps = ParseRemoteCrDepsFile(current_cr_rev)
  new_cr_deps = ParseRemoteCrDepsFile(new_cr_rev)

  if new_commit_pos > current_commit_pos or opts.allow_reverse:
    changed_deps = sorted(CalculateChangedDeps(current_cr_deps, new_cr_deps))
    clang_change = CalculateChangedClang(new_cr_rev)
    commit_msg = GenerateCommitMessage(current_cr_rev, new_cr_rev,
                                       current_commit_pos, new_commit_pos,
                                       changed_deps, clang_change)
    logging.debug('Commit message:\n%s', commit_msg)
  else:
    logging.info('Currently pinned chromium_revision: %s (#%s) is newer than '
                 '%s (#%s). To roll to older revisions, you must pass the '
                 '--allow-reverse flag.\n'
                 'Aborting without action.', current_cr_rev, current_commit_pos,
                 new_cr_rev, new_commit_pos)
    return 0

  _CreateRollBranch(opts.dry_run)
  UpdateDeps(deps_filename, current_cr_rev, new_cr_rev)
  _LocalCommit(commit_msg, opts.dry_run)
  _UploadCL(opts.dry_run, opts.rietveld_email)
  _SendToCQ(opts.dry_run, opts.skip_cq)
  return 0


if __name__ == '__main__':
  sys.exit(main())
