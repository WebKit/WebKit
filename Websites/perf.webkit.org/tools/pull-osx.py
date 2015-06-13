#!/usr/bin/python

import argparse
import json
import sys
import time
import urllib
import urllib2

from xml.dom.minidom import parseString as parseXmlString
from util import submit_commits
from util import text_content


HTTP_AUTH_HANDLERS = {
    'basic': urllib2.HTTPBasicAuthHandler,
    'digest': urllib2.HTTPDigestAuthHandler,
}


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True, help='Path to a config JSON file')
    args = parser.parse_args()

    with open(args.config) as config_file:
        config = json.load(config_file)

    setup_auth(config['server'])

    url = config['buildSourceURL']
    submission_size = config['submissionSize']
    reported_revisions = set()

    while True:
        print "Fetching available builds from %s" % url
        available_builds = fetch_available_builds(url, config['trainVersionMap'])
        available_builds = filter(lambda commit: commit['revision'] not in reported_revisions, available_builds)
        print "%d builds available" % len(available_builds)

        while True:
            commits_to_submit = available_builds[:submission_size]
            revisions_to_report = map(lambda commit: commit['revision'], commits_to_submit)
            print "Submitting builds (%d remaining):" % len(available_builds), json.dumps(revisions_to_report)
            available_builds = available_builds[submission_size:]

            submit_commits(commits_to_submit, config['server']['url'], config['slave']['name'], config['slave']['password'])
            reported_revisions |= set(revisions_to_report)

            time.sleep(config['submissionInterval'])

        time.sleep(config['fetchInterval'])


def setup_auth(server):
    auth = server.get('auth')
    if not auth:
        return

    password_manager = urllib2.HTTPPasswordMgr()
    password_manager.add_password(realm=auth['realm'], uri=server['url'], user=auth['username'], passwd=auth['password'])
    auth_handler = HTTP_AUTH_HANDLERS[auth['type']](password_manager)
    urllib2.install_opener(urllib2.build_opener(auth_handler))


def fetch_available_builds(url, train_version_map):
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
            'repository': 'OS X',
            'revision': train_version_map[train] + ' ' + build})

    return available_builds


if __name__ == "__main__":
    main(sys.argv)
