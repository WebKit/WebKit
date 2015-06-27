#!/usr/bin/python

import argparse
import json
import re
import sys
import subprocess
import time
import urllib
import urllib2

from xml.dom.minidom import parseString as parseXmlString
from util import submit_commits
from util import text_content
from util import setup_auth


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True, help='Path to a config JSON file')
    args = parser.parse_args()

    with open(args.config) as config_file:
        config = json.load(config_file)

    setup_auth(config['server'])

    submission_size = config['submissionSize']
    reported_revisions = set()

    while True:
        if 'customCommands' in config:
            available_builds = []
            for command in config['customCommands']:
                print "Executing", ' '.join(command['command'])
                available_builds += available_builds_from_command(config['repositoryName'], command['command'], command['linesToIgnore'])
                print "Got %d builds" % len(available_builds)
        else:
            url = config['buildSourceURL']
            print "Fetching available builds from", url
            available_builds = fetch_available_builds(config['repositoryName'], url, config['trainVersionMap'])

        available_builds = filter(lambda commit: commit['revision'] not in reported_revisions, available_builds)
        print "%d builds available" % len(available_builds)

        while available_builds:
            commits_to_submit = available_builds[:submission_size]
            revisions_to_report = map(lambda commit: commit['revision'], commits_to_submit)
            print "Submitting builds (%d remaining):" % len(available_builds), json.dumps(revisions_to_report)
            available_builds = available_builds[submission_size:]

            submit_commits(commits_to_submit, config['server']['url'], config['slave']['name'], config['slave']['password'])
            reported_revisions |= set(revisions_to_report)

            time.sleep(config['submissionInterval'])

        print "Sleeping for %d seconds" % config['fetchInterval']
        time.sleep(config['fetchInterval'])


def available_builds_from_command(repository_name, command, lines_to_ignore):
    try:
        output = subprocess.check_output(command, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as error:
        print "Failed:", str(error)

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
