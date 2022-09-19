#!/usr/bin/env vpython3

# Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script to auto-update the WebRTC source version in call/version.cc"""

import argparse
import datetime
import logging
import os
import re
import subprocess
import sys


def FindSrcDirPath():
  """Returns the abs path to the src/ dir of the project."""
  src_dir = os.path.dirname(os.path.abspath(__file__))
  while os.path.basename(src_dir) != 'src':
    src_dir = os.path.normpath(os.path.join(src_dir, os.pardir))
  return src_dir


UPDATE_BRANCH_NAME = 'webrtc_version_update'
CHECKOUT_SRC_DIR = FindSrcDirPath()

NOTIFY_EMAIL = 'webrtc-trooper@webrtc.org'


def _RemovePreviousUpdateBranch():
  active_branch, branches = _GetBranches()
  if active_branch == UPDATE_BRANCH_NAME:
    active_branch = 'main'
  if UPDATE_BRANCH_NAME in branches:
    logging.info('Removing previous update branch (%s)', UPDATE_BRANCH_NAME)
    subprocess.check_call(['git', 'checkout', active_branch])
    subprocess.check_call(['git', 'branch', '-D', UPDATE_BRANCH_NAME])
  logging.info('No branch to remove')


def _GetLastAuthor():
  """Returns a string with the author of the last commit."""
  author = subprocess.check_output(
      ['git', 'log', '-1', '--pretty=format:"%an"'],
      universal_newlines=True).splitlines()
  return author


def _GetBranches():
  """Returns a tuple (active, branches).

    'active' is a string with name of the currently active branch, while
     'branches' is the list of all branches.
    """
  lines = subprocess.check_output(['git', 'branch'],
                                  universal_newlines=True).splitlines()
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


def _CreateUpdateBranch():
  logging.info('Creating update branch: %s', UPDATE_BRANCH_NAME)
  subprocess.check_call(['git', 'checkout', '-b', UPDATE_BRANCH_NAME])


def _UpdateWebRTCVersion(filename):
  with open(filename, 'rb') as f:
    content = f.read().decode('utf-8')
  d = datetime.datetime.utcnow()
  # pylint: disable=line-too-long
  new_content = re.sub(
      r'WebRTC source stamp [0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}',
      r'WebRTC source stamp %02d-%02d-%02dT%02d:%02d:%02d' %
      (d.year, d.month, d.day, d.hour, d.minute, d.second),
      content,
      flags=re.MULTILINE)
  # pylint: enable=line-too-long
  with open(filename, 'wb') as f:
    f.write(new_content.encode('utf-8'))


def _IsTreeClean():
  stdout = subprocess.check_output(['git', 'status', '--porcelain'],
                                   universal_newlines=True)
  if len(stdout) == 0:
    return True
  return False


def _LocalCommit():
  logging.info('Committing changes locally.')
  d = datetime.datetime.utcnow()

  commit_msg = ('Update WebRTC code version (%02d-%02d-%02dT%02d:%02d:%02d).'
                '\n\nBug: None')
  commit_msg = commit_msg % (d.year, d.month, d.day, d.hour, d.minute, d.second)
  subprocess.check_call(['git', 'add', '--update', '.'])
  subprocess.check_call(['git', 'commit', '-m', commit_msg])


def _UploadCL(commit_queue_mode):
  """Upload the committed changes as a changelist to Gerrit.

  commit_queue_mode:
    - 2: Submit to commit queue.
    - 1: Run trybots but do not submit to CQ.
    - 0: Skip CQ, upload only.
  """
  cmd = [
      'git', 'cl', 'upload', '--force', '--bypass-hooks', '--bypass-watchlist'
  ]
  if commit_queue_mode >= 2:
    logging.info('Sending the CL to the CQ...')
    cmd.extend(['-o', 'label=Bot-Commit+1'])
    cmd.extend(['-o', 'label=Commit-Queue+2'])
    cmd.extend(['--send-mail', '--cc', NOTIFY_EMAIL])
  elif commit_queue_mode >= 1:
    logging.info('Starting CQ dry run...')
    cmd.extend(['-o', 'label=Commit-Queue+1'])
  subprocess.check_call(cmd)


def main():
  logging.basicConfig(level=logging.INFO)
  p = argparse.ArgumentParser()
  p.add_argument('--clean',
                 action='store_true',
                 default=False,
                 help='Removes any previous local update branch.')
  opts = p.parse_args()

  if opts.clean:
    _RemovePreviousUpdateBranch()

  if _GetLastAuthor() == 'webrtc-version-updater':
    logging.info('Last commit is a version change, skipping CL.')
    return 0

  version_filename = os.path.join(CHECKOUT_SRC_DIR, 'call', 'version.cc')
  _CreateUpdateBranch()
  _UpdateWebRTCVersion(version_filename)
  if _IsTreeClean():
    logging.info('No WebRTC version change detected, skipping CL.')
  else:
    _LocalCommit()
    logging.info('Uploading CL...')
    _UploadCL(2)
  return 0


if __name__ == '__main__':
  sys.exit(main())
