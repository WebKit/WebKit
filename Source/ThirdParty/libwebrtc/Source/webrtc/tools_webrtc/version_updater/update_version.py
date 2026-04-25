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


UPDATE_BRANCH_NAME = 'webrtc_version_update'
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHECKOUT_SRC_DIR = os.path.realpath(
  os.path.join(SCRIPT_DIR, os.pardir, os.pardir))

NOTIFY_EMAIL = 'webrtc-trooper@webrtc.org'


def _remove_previous_update_branch():
    active_branch, branches = _get_branches()
    if active_branch == UPDATE_BRANCH_NAME:
        active_branch = 'main'
    if UPDATE_BRANCH_NAME in branches:
        logging.info('Removing previous update branch (%s)',
                     UPDATE_BRANCH_NAME)
        subprocess.check_call(['git', 'checkout', active_branch])
        subprocess.check_call(['git', 'branch', '-D', UPDATE_BRANCH_NAME])
    logging.info('No branch to remove')


def _get_last_author():
    """Returns a string with the author of the last commit."""
    author = subprocess.check_output(
        ['git', 'log', '-1', '--pretty=format:"%an"'],
        universal_newlines=True).splitlines()
    return author


def _get_branches():
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


def _create_update_branch():
    logging.info('Creating update branch: %s', UPDATE_BRANCH_NAME)
    subprocess.check_call(['git', 'checkout', '-b', UPDATE_BRANCH_NAME])


def _update_webrtc_version(filename):
    with open(filename, 'rb') as file:
        content = file.read().decode('utf-8')
    date = datetime.datetime.utcnow()
    # pylint: disable=line-too-long
    new_content = re.sub(
        r'WebRTC source stamp [0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}',
        r'WebRTC source stamp %02d-%02d-%02dT%02d:%02d:%02d' %
        (date.year, date.month, date.day, date.hour, date.minute, date.second),
        content,
        flags=re.MULTILINE)
    # pylint: enable=line-too-long
    with open(filename, 'wb') as file:
        file.write(new_content.encode('utf-8'))


def _is_tree_clean():
    stdout = subprocess.check_output(['git', 'status', '--porcelain'],
                                     universal_newlines=True)
    if len(stdout) == 0:
        return True
    return False


def _local_commit():
    logging.info('Committing changes locally.')
    date = datetime.datetime.utcnow()

    msg = ('Update WebRTC code version (%02d-%02d-%02dT%02d:%02d:%02d).'
           '\n\nBug: None')
    msg = msg % (date.year, date.month, date.day, date.hour, date.minute,
                 date.second)
    subprocess.check_call(['git', 'add', '--update', '.'])
    subprocess.check_call(['git', 'commit', '-m', msg])


def _upload_cl(commit_queue_mode):
    """Upload the committed changes as a changelist to Gerrit.

  commit_queue_mode:
    - 2: Submit to commit queue.
    - 1: Run trybots but do not submit to CQ.
    - 0: Skip CQ, upload only.
  """
    cmd = [
        'git', 'cl', 'upload', '--force', '--bypass-hooks',
        '--bypass-watchlist'
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
    parser = argparse.ArgumentParser()
    parser.add_argument('--clean',
                        action='store_true',
                        default=False,
                        help='Removes any previous local update branch.')
    opts = parser.parse_args()

    if opts.clean:
        _remove_previous_update_branch()

    if _get_last_author() == 'webrtc-version-updater':
        logging.info('Last commit is a version change, skipping CL.')
        return 0

    version_filename = os.path.join(CHECKOUT_SRC_DIR, 'call', 'version.cc')
    _create_update_branch()
    _update_webrtc_version(version_filename)
    if _is_tree_clean():
        logging.info('No WebRTC version change detected, skipping CL.')
    else:
        _local_commit()
        logging.info('Uploading CL...')
        _upload_cl(2)
    return 0


if __name__ == '__main__':
    sys.exit(main())
