#!/usr/bin/python

import argparse
import json
import operator
import re
import sys
import subprocess
import time
import urllib
import urllib2

from datetime import datetime
from xml.dom.minidom import parseString as parseXmlString
from util import submit_commits
from util import text_content
from util import load_server_config


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--os-config-json', required=True, help='The path to a JSON that specifies how to fetch OS build information')
    parser.add_argument('--server-config-json', required=True, help='The path to a JSON file that specifies the perf dashboard')
    parser.add_argument('--seconds-to-sleep', type=float, default=43200, help='The seconds to sleep between iterations')
    args = parser.parse_args()

    with open(args.os_config_json) as os_config_json:
        os_config_list = json.load(os_config_json)

    fetchers = [OSBuildFetcher(os_config) for os_config in os_config_list]

    while True:
        server_config = load_server_config(args.server_config_json)
        for fetcher in fetchers:
            fetcher.fetch_and_report_new_builds(server_config)
        print "Sleeping for %d seconds" % args.seconds_to_sleep
        time.sleep(args.seconds_to_sleep)


# FIXME: Move other static functions into this class.
class OSBuildFetcher:
    def __init__(self, os_config):
        self._os_config = os_config
        self._reported_revisions = set()

    def _fetch_available_builds(self):
        config = self._os_config
        repository_name = self._os_config['name']

        if 'customCommands' in config:
            available_builds = []
            for command in config['customCommands']:
                print "Executing", ' '.join(command['command'])
                available_builds += available_builds_from_command(repository_name, command['command'], command['linesToIgnore'])
        else:
            url = config['buildSourceURL']
            print "Fetching available builds from", url
            available_builds = fetch_available_builds(repository_name, url, config['trainVersionMap'])
        return available_builds

    def fetch_and_report_new_builds(self, server_config):
        available_builds = self._fetch_available_builds()
        reported_revisions = self._reported_revisions
        print 'Found %d builds' % len(available_builds)

        available_builds = filter(lambda commit: commit['revision'] not in reported_revisions, available_builds)
        self._assign_order(available_builds)

        print "Submitting %d builds" % len(available_builds)
        submit_commits(available_builds, server_config['server']['url'], server_config['slave']['name'], server_config['slave']['password'])
        reported_revisions |= set(map(lambda commit: commit['revision'], available_builds))

    @staticmethod
    def _assign_order(builds):
        build_name_regex = re.compile(r'(?P<major>\d+)(?P<kind>\w)(?P<minor>\d+)(?P<variant>\w*)')
        for commit in builds:
            match = build_name_regex.search(commit['revision'])
            major = int(match.group('major'))
            kind = ord(match.group('kind').upper()) - ord('A')
            minor = int(match.group('minor'))
            variant = ord(match.group('variant').upper()) - ord('A') + 1 if match.group('variant') else 0
            commit['order'] = ((major * 100 + kind) * 10000 + minor) * 100 + variant


def available_builds_from_command(repository_name, command, lines_to_ignore):
    try:
        output = subprocess.check_output(command, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as error:
        print "Failed:", str(error)
        return []

    regex = re.compile(lines_to_ignore)
    return [{'repository': repository_name, 'revision': line} for line in output.split('\n') if not regex.match(line)]


def fetch_available_builds(repository_name, url, train_version_map):
    output = urllib2.urlopen(url).read()
    try:
        xml = parseXmlString(output)
    except Exception, error:
        raise Exception(error, output)
    available_builds = []
    for image in xml.getElementsByTagName('baseASR'):
        id = text_content(image.getElementsByTagName('id')[0])
        train = text_content(image.getElementsByTagName('train')[0])
        build = text_content(image.getElementsByTagName('build')[0])
        if train not in train_version_map:
            continue
        available_builds.append({
            'repository': repository_name,
            'revision': train_version_map[train] + ' ' + build})

    return available_builds


if __name__ == "__main__":
    main(sys.argv)
