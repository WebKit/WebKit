#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script for publishing WebRTC AAR on Bintray.

Set BINTRAY_USER and BINTRAY_API_KEY environment variables before running
this script for authentication.
"""

import argparse
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time


SCRIPT_DIR = os.path.dirname(os.path.realpath(sys.argv[0]))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))

sys.path.append(os.path.join(CHECKOUT_ROOT, 'third_party'))
import requests
import jinja2

sys.path.append(os.path.join(CHECKOUT_ROOT, 'tools_webrtc'))
from android.build_aar import BuildAar


ARCHS = ['armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64']
MAVEN_REPOSITORY = 'https://google.bintray.com/webrtc'
API = 'https://api.bintray.com'
PACKAGE_PATH = 'google/webrtc/google-webrtc'
CONTENT_API = API + '/content/' + PACKAGE_PATH
PACKAGES_API = API + '/packages/' + PACKAGE_PATH
GROUP_ID = 'org/webrtc'
ARTIFACT_ID = 'google-webrtc'
COMMIT_POSITION_REGEX = r'^Cr-Commit-Position: refs/heads/master@{#(\d+)}$'
API_TIMEOUT_SECONDS = 10.0
UPLOAD_TRIES = 3
# The sleep time is increased exponentially.
UPLOAD_RETRY_BASE_SLEEP_SECONDS = 2.0
GRADLEW_BIN = os.path.join(CHECKOUT_ROOT,
                           'examples/androidtests/third_party/gradle/gradlew')
AAR_PROJECT_DIR = os.path.join(CHECKOUT_ROOT, 'examples/aarproject')
AAR_PROJECT_GRADLE = os.path.join(AAR_PROJECT_DIR, 'build.gradle')
AAR_PROJECT_APP_GRADLE = os.path.join(AAR_PROJECT_DIR, 'app', 'build.gradle')
AAR_PROJECT_DEPENDENCY = "implementation 'org.webrtc:google-webrtc:1.0.+'"
AAR_PROJECT_VERSION_DEPENDENCY = "implementation 'org.webrtc:google-webrtc:%s'"


def _ParseArgs():
  parser = argparse.ArgumentParser(description='Releases WebRTC on Bintray.')
  parser.add_argument('--use-goma', action='store_true', default=False,
      help='Use goma.')
  parser.add_argument('--skip-tests', action='store_true', default=False,
      help='Skips running the tests.')
  parser.add_argument('--publish', action='store_true', default=False,
      help='Automatically publishes the library if the tests pass.')
  parser.add_argument('--verbose', action='store_true', default=False,
      help='Debug logging.')
  return parser.parse_args()


def _GetCommitHash():
  commit_hash = subprocess.check_output(
    ['git', 'rev-parse', 'HEAD'], cwd=CHECKOUT_ROOT).strip()
  return commit_hash


def _GetCommitPos():
  commit_message = subprocess.check_output(
      ['git', 'rev-list', '--format=%B', '--max-count=1', 'HEAD'],
      cwd=CHECKOUT_ROOT)
  commit_pos_match = re.search(
      COMMIT_POSITION_REGEX, commit_message, re.MULTILINE)
  if not commit_pos_match:
    raise Exception('Commit position not found in the commit message: %s'
                      % commit_message)
  return commit_pos_match.group(1)


def _UploadFile(user, password, filename, version, target_file):
# URL is of format:
  # <repository_api>/<version>/<group_id>/<artifact_id>/<version>/<target_file>
  # Example:
  # https://api.bintray.com/content/google/webrtc/google-webrtc/1.0.19742/org/webrtc/google-webrtc/1.0.19742/google-webrtc-1.0.19742.aar

  target_dir = version + '/' + GROUP_ID + '/' + ARTIFACT_ID + '/' + version
  target_path = target_dir + '/' + target_file
  url = CONTENT_API + '/' + target_path

  logging.info('Uploading %s to %s', filename, url)
  with open(filename) as fh:
    file_data = fh.read()

  for attempt in xrange(UPLOAD_TRIES):
    try:
      response = requests.put(url, data=file_data, auth=(user, password),
                              timeout=API_TIMEOUT_SECONDS)
      break
    except requests.exceptions.Timeout as e:
      logging.warning('Timeout while uploading: %s', e)
      time.sleep(UPLOAD_RETRY_BASE_SLEEP_SECONDS ** attempt)
  else:
    raise Exception('Failed to upload %s' % filename)

  if not response.ok:
    raise Exception('Failed to upload %s. Response: %s' % (filename, response))
  logging.info('Uploaded %s: %s', filename, response)


def _GeneratePom(target_file, version, commit):
  env = jinja2.Environment(
    loader=jinja2.PackageLoader('release_aar'),
  )
  template = env.get_template('pom.jinja')
  pom = template.render(version=version, commit=commit)
  with open(target_file, 'w') as fh:
    fh.write(pom)


def _TestAAR(tmp_dir, username, password, version):
  """Runs AppRTCMobile tests using the AAR. Returns true if the tests pass."""
  logging.info('Testing library.')
  env = jinja2.Environment(
    loader=jinja2.PackageLoader('release_aar'),
  )

  gradle_backup = os.path.join(tmp_dir, 'build.gradle.backup')
  app_gradle_backup = os.path.join(tmp_dir, 'app-build.gradle.backup')

  # Make backup copies of the project files before modifying them.
  shutil.copy2(AAR_PROJECT_GRADLE, gradle_backup)
  shutil.copy2(AAR_PROJECT_APP_GRADLE, app_gradle_backup)

  try:
    maven_repository_template = env.get_template('maven-repository.jinja')
    maven_repository = maven_repository_template.render(
        url=MAVEN_REPOSITORY, username=username, password=password)

    # Append Maven repository to build file to download unpublished files.
    with open(AAR_PROJECT_GRADLE, 'a') as gradle_file:
      gradle_file.write(maven_repository)

    # Read app build file.
    with open(AAR_PROJECT_APP_GRADLE, 'r') as gradle_app_file:
      gradle_app = gradle_app_file.read()

    if AAR_PROJECT_DEPENDENCY not in gradle_app:
      raise Exception(
          '%s not found in the build file.' % AAR_PROJECT_DEPENDENCY)
    # Set version to the version to be tested.
    target_dependency = AAR_PROJECT_VERSION_DEPENDENCY % version
    gradle_app = gradle_app.replace(AAR_PROJECT_DEPENDENCY, target_dependency)

    # Write back.
    with open(AAR_PROJECT_APP_GRADLE, 'w') as gradle_app_file:
      gradle_app_file.write(gradle_app)

    # Run tests.
    try:
      # First clean the project.
      subprocess.check_call([GRADLEW_BIN, 'clean'], cwd=AAR_PROJECT_DIR)
      # Then run the tests.
      subprocess.check_call([GRADLEW_BIN, 'connectedDebugAndroidTest'],
                            cwd=AAR_PROJECT_DIR)
    except subprocess.CalledProcessError:
      logging.exception('Test failure.')
      return False  # Clean or tests failed

    return True  # Tests pass
  finally:
    # Restore backups.
    shutil.copy2(gradle_backup, AAR_PROJECT_GRADLE)
    shutil.copy2(app_gradle_backup, AAR_PROJECT_APP_GRADLE)


def _PublishAAR(user, password, version, additional_args):
  args = {
    'publish_wait_for_secs': 0  # Publish asynchronously.
  }
  args.update(additional_args)

  url = CONTENT_API + '/' + version + '/publish'
  response = requests.post(url, data=json.dumps(args), auth=(user, password),
                           timeout=API_TIMEOUT_SECONDS)

  if not response.ok:
    raise Exception('Failed to publish. Response: %s' % response)


def _DeleteUnpublishedVersion(user, password, version):
  url = PACKAGES_API + '/versions/' + version
  response = requests.get(url, auth=(user, password),
                          timeout=API_TIMEOUT_SECONDS)
  if not response.ok:
    raise Exception('Failed to get version info. Response: %s' % response)

  version_info = json.loads(response.content)
  if version_info['published']:
    logging.info('Version has already been published, not deleting.')
    return

  logging.info('Deleting unpublished version.')
  response = requests.delete(url, auth=(user, password),
                             timeout=API_TIMEOUT_SECONDS)
  if not response.ok:
    raise Exception('Failed to delete version. Response: %s' % response)


def ReleaseAar(use_goma, skip_tests, publish):
  version = '1.0.' + _GetCommitPos()
  commit = _GetCommitHash()
  logging.info('Releasing AAR version %s with hash %s', version, commit)

  user = os.environ.get('BINTRAY_USER', None)
  api_key = os.environ.get('BINTRAY_API_KEY', None)
  if not user or not api_key:
    raise Exception('Environment variables BINTRAY_USER and BINTRAY_API_KEY '
                    'must be defined.')

  tmp_dir = tempfile.mkdtemp()

  try:
    base_name = ARTIFACT_ID + '-' + version
    aar_file = os.path.join(tmp_dir, base_name + '.aar')
    third_party_licenses_file = os.path.join(tmp_dir, 'LICENSE.md')
    pom_file = os.path.join(tmp_dir, base_name + '.pom')

    logging.info('Building at %s', tmp_dir)
    BuildAar(ARCHS, aar_file,
             use_goma=use_goma,
             ext_build_dir=os.path.join(tmp_dir, 'aar-build'))
    _GeneratePom(pom_file, version, commit)

    _UploadFile(user, api_key, aar_file, version, base_name + '.aar')
    _UploadFile(user, api_key, third_party_licenses_file, version,
                'THIRD_PARTY_LICENSES.md')
    _UploadFile(user, api_key, pom_file, version, base_name + '.pom')

    tests_pass = skip_tests or _TestAAR(tmp_dir, user, api_key, version)
    if not tests_pass:
      logging.info('Discarding library.')
      _PublishAAR(user, api_key, version, {'discard': True})
      _DeleteUnpublishedVersion(user, api_key, version)
      raise Exception('Test failure. Discarded library.')

    if publish:
      logging.info('Publishing library.')
      _PublishAAR(user, api_key, version, {})
    else:
      logging.info('Note: The library has not not been published automatically.'
                   ' Please do so manually if desired.')
  finally:
    shutil.rmtree(tmp_dir, True)


def main():
  args = _ParseArgs()
  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)
  ReleaseAar(args.use_goma, args.skip_tests, args.publish)


if __name__ == '__main__':
  sys.exit(main())
