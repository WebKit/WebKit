#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script to automatically roll dependencies in the WebRTC DEPS file."""

import argparse
import base64
import collections
import logging
import os
import re
import subprocess
import sys
import urllib2


# Skip these dependencies (list without solution name prefix).
DONT_AUTOROLL_THESE = [
  'src/third_party/winsdk_samples',
  'src/examples/androidtests/third_party/gradle',
]

# Run these CQ trybots in addition to the default ones in infra/config/cq.cfg.
EXTRA_TRYBOTS = (
    'master.internal.tryserver.corp.webrtc:linux_internal'
)

WEBRTC_URL = 'https://webrtc.googlesource.com/src'
CHROMIUM_SRC_URL = 'https://chromium.googlesource.com/chromium/src'
CHROMIUM_COMMIT_TEMPLATE = CHROMIUM_SRC_URL + '/+/%s'
CHROMIUM_LOG_TEMPLATE = CHROMIUM_SRC_URL + '/+log/%s'
CHROMIUM_FILE_TEMPLATE = CHROMIUM_SRC_URL + '/+/%s/%s'

COMMIT_POSITION_RE = re.compile('^Cr-Commit-Position: .*#([0-9]+).*$')
CLANG_REVISION_RE = re.compile(r'^CLANG_REVISION = \'(\d+)\'$')
ROLL_BRANCH_NAME = 'roll_chromium_revision'

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHECKOUT_SRC_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, os.pardir,
                                                 os.pardir))
CHECKOUT_ROOT_DIR = os.path.realpath(os.path.join(CHECKOUT_SRC_DIR, os.pardir))

sys.path.append(os.path.join(CHECKOUT_SRC_DIR, 'build'))
import find_depot_tools
find_depot_tools.add_depot_tools_to_path()

CLANG_UPDATE_SCRIPT_URL_PATH = 'tools/clang/scripts/update.py'
CLANG_UPDATE_SCRIPT_LOCAL_PATH = os.path.join(CHECKOUT_SRC_DIR, 'tools',
                                              'clang', 'scripts', 'update.py')

DepsEntry = collections.namedtuple('DepsEntry', 'path url revision')
ChangedDep = collections.namedtuple('ChangedDep',
                                    'path url current_rev new_rev')

class RollError(Exception):
  pass


def VarLookup(local_scope):
  return lambda var_name: local_scope['vars'][var_name]


def ParseDepsDict(deps_content):
  local_scope = {}
  global_scope = {
    'Var': VarLookup(local_scope),
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
  working_dir = working_dir or CHECKOUT_SRC_DIR
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
  conn = urllib2.urlopen(url)
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
  """Builds a dict of paths to DepsEntry objects from a raw parsed deps dict."""
  result = {}
  def AddDepsEntries(deps_subdict):
    for path, deps_url in deps_subdict.iteritems():
      if isinstance(deps_url, dict):
        deps_url = deps_url['url']
      if not result.has_key(path):
        url, revision = deps_url.split('@') if deps_url else (None, None)
        result[path] = DepsEntry(path, url, revision)

  AddDepsEntries(deps_dict['deps'])
  for deps_os in ['win', 'mac', 'unix', 'android', 'ios', 'unix']:
    AddDepsEntries(deps_dict.get('deps_os', {}).get(deps_os, {}))
  return result


def CalculateChangedDeps(webrtc_deps, new_cr_deps):
  """
  Calculate changed deps entries based on entries defined in the WebRTC DEPS
  file:
     - If a shared dependency with the Chromium DEPS file: roll it to the same
       revision as Chromium (i.e. entry in the new_cr_deps dict)
     - If it's a Chromium sub-directory, roll it to the HEAD revision (notice
       this means it may be ahead of the chromium_revision, but generally these
       should be close).
     - If it's another DEPS entry (not shared with Chromium), roll it to HEAD
       unless it's configured to be skipped.

  Returns:
    A list of ChangedDep objects representing the changed deps.
  """
  result = []
  webrtc_entries = BuildDepsentryDict(webrtc_deps)
  new_cr_entries = BuildDepsentryDict(new_cr_deps)
  for path, webrtc_deps_entry in webrtc_entries.iteritems():
    if path in DONT_AUTOROLL_THESE:
      continue
    cr_deps_entry = new_cr_entries.get(path)
    if cr_deps_entry:
      # Use the revision from Chromium's DEPS file.
      new_rev = cr_deps_entry.revision
      assert webrtc_deps_entry.url == cr_deps_entry.url, (
        'WebRTC DEPS entry %s has a different URL (%s) than Chromium (%s).' %
        (path, webrtc_deps_entry.url, cr_deps_entry.url))
    else:
      # Use the HEAD of the deps repo.
      stdout, _ = _RunCommand(['git', 'ls-remote', webrtc_deps_entry.url,
                               'HEAD'])
      new_rev = stdout.strip().split('\t')[0]

    # Check if an update is necessary.
    if webrtc_deps_entry.revision != new_rev:
      logging.debug('Roll dependency %s to %s', path, new_rev)
      result.append(ChangedDep(path, webrtc_deps_entry.url,
                               webrtc_deps_entry.revision, new_rev))
  return sorted(result)


def CalculateChangedClang(new_cr_rev):
  def GetClangRev(lines):
    for line in lines:
      match = CLANG_REVISION_RE.match(line)
      if match:
        return match.group(1)
    raise RollError('Could not parse Clang revision!')

  with open(CLANG_UPDATE_SCRIPT_LOCAL_PATH, 'rb') as f:
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

  # TBR needs to be non-empty for Gerrit to process it.
  git_author = _RunCommand(['git', 'config', 'user.email'],
                           working_dir=CHECKOUT_SRC_DIR)[0].splitlines()[0]
  tbr_authors = git_author + ',' + tbr_authors

  commit_msg.append('TBR=%s' % tbr_authors)
  commit_msg.append('BUG=None')
  commit_msg.append('CQ_INCLUDE_TRYBOTS=%s' % EXTRA_TRYBOTS)
  return '\n'.join(commit_msg)


def UpdateDepsFile(deps_filename, old_cr_revision, new_cr_revision,
                   changed_deps):
  """Update the DEPS file with the new revision."""

  # Update the chromium_revision variable.
  with open(deps_filename, 'rb') as deps_file:
    deps_content = deps_file.read()
  deps_content = deps_content.replace(old_cr_revision, new_cr_revision)
  with open(deps_filename, 'wb') as deps_file:
    deps_file.write(deps_content)

  # Update each individual DEPS entry.
  for dep in changed_deps:
    local_dep_dir = os.path.join(CHECKOUT_ROOT_DIR, dep.path)
    if not os.path.isdir(local_dep_dir):
      raise RollError(
          'Cannot find local directory %s. Either run\n'
          'gclient sync --deps=all\n'
          'or make sure the .gclient file for your solution contains all '
          'platforms in the target_os list, i.e.\n'
          'target_os = ["android", "unix", "mac", "ios", "win"];\n'
          'Then run "gclient sync" again.' % local_dep_dir)
    _, stderr = _RunCommand(
      ['roll-dep-svn', '--no-verify-revision', dep.path, dep.new_rev],
      working_dir=CHECKOUT_SRC_DIR, ignore_exit_code=True)
    if stderr:
      logging.warning('roll-dep-svn: %s', stderr)


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


def _UploadCL(dry_run, skip_cq=False):
  logging.info('Uploading CL...')
  if not dry_run:
    cmd = ['git', 'cl', 'upload', '-f', '--gerrit']
    if not skip_cq:
      logging.info('Sending the CL to the CQ...')
      cmd.extend(['--use-commit-queue', '--send-mail'])
    _RunCommand(cmd, extra_env={'EDITOR': 'true', 'SKIP_GCE_AUTH_FOR_GIT': '1'})


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
  p.add_argument('-i', '--ignore-unclean-workdir', action='store_true',
                 default=False,
                 help=('Ignore if the current branch is not master or if there '
                       'are uncommitted changes (default: %(default)s).'))
  p.add_argument('--skip-cq', action='store_true', default=False,
                 help='Skip sending the CL to the CQ (default: %(default)s)')
  p.add_argument('-v', '--verbose', action='store_true', default=False,
                 help='Be extra verbose in printing of log messages.')
  opts = p.parse_args()

  if opts.verbose:
    logging.basicConfig(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.INFO)

  if not opts.ignore_unclean_workdir and not _IsTreeClean():
    logging.error('Please clean your local checkout first.')
    return 1

  if opts.clean:
    _RemovePreviousRollBranch(opts.dry_run)

  if not opts.ignore_unclean_workdir:
    _EnsureUpdatedMasterBranch(opts.dry_run)

  new_cr_rev = opts.revision
  if not new_cr_rev:
    stdout, _ = _RunCommand(['git', 'ls-remote', CHROMIUM_SRC_URL, 'HEAD'])
    head_rev = stdout.strip().split('\t')[0]
    logging.info('No revision specified. Using HEAD: %s', head_rev)
    new_cr_rev = head_rev

  deps_filename = os.path.join(CHECKOUT_SRC_DIR, 'DEPS')
  webrtc_deps = ParseLocalDepsFile(deps_filename)
  current_cr_rev = webrtc_deps['vars']['chromium_revision']

  current_commit_pos = ParseCommitPosition(ReadRemoteCrCommit(current_cr_rev))
  new_commit_pos = ParseCommitPosition(ReadRemoteCrCommit(new_cr_rev))

  new_cr_deps = ParseRemoteCrDepsFile(new_cr_rev)
  changed_deps = CalculateChangedDeps(webrtc_deps, new_cr_deps)
  clang_change = CalculateChangedClang(new_cr_rev)
  commit_msg = GenerateCommitMessage(current_cr_rev, new_cr_rev,
                                     current_commit_pos, new_commit_pos,
                                     changed_deps, clang_change)
  logging.debug('Commit message:\n%s', commit_msg)

  _CreateRollBranch(opts.dry_run)
  UpdateDepsFile(deps_filename, current_cr_rev, new_cr_rev, changed_deps)
  if _IsTreeClean():
    logging.info("No DEPS changes detected, skipping CL creation.")
  else:
    _LocalCommit(commit_msg, opts.dry_run)
    _UploadCL(opts.dry_run, opts.skip_cq)
  return 0


if __name__ == '__main__':
  sys.exit(main())
