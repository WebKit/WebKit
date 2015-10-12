#!/usr/bin/python

import argparse
import json
import re
import subprocess
import sys
import time
import urllib2

from xml.dom.minidom import parseString as parseXmlString
from util import load_server_config
from util import submit_commits
from util import text_content


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--svn-config-json', required=True, help='The path to a JSON file that specifies subversion syncing options')
    parser.add_argument('--server-config-json', required=True, help='The path to a JSON file that specifies the perf dashboard')
    parser.add_argument('--seconds-to-sleep', type=float, default=900, help='The seconds to sleep between iterations')
    parser.add_argument('--max-fetch-count', type=int, default=10, help='The number of commits to fetch at once')
    args = parser.parse_args()

    with open(args.svn_config_json) as svn_config_json:
        svn_config = json.load(svn_config_json)

    while True:
        server_config = load_server_config(args.server_config_json)
        for repository_info in svn_config:
            fetch_commits_and_submit(repository_info, server_config, args.max_fetch_count)
        print "Sleeping for %d seconds..." % args.seconds_to_sleep
        time.sleep(args.seconds_to_sleep)


def fetch_commits_and_submit(repository, server_config, max_fetch_count):
    assert 'name' in repository, 'The repository name should be specified'
    assert 'url' in repository, 'The SVN repository URL should be specified'

    if 'revisionToFetch' not in repository:
        print "Determining the stating revision for %s" % repository['name']
        repository['revisionToFecth'] = determine_first_revision_to_fetch(server_config['server']['url'], repository['name'])

    if 'useServerAuth' in repository:
        repository['username'] = server_config['server']['auth']['username']
        repository['password'] = server_config['server']['auth']['password']

    pending_commits = []
    for unused in range(max_fetch_count):
        commit = fetch_commit_and_resolve_author(repository, repository.get('accountNameFinderScript', None), repository['revisionToFecth'])
        if not commit:
            break
        pending_commits += [commit]
        repository['revisionToFecth'] += 1

    if not pending_commits:
        print "No new revision found for %s (waiting for r%d)" % (repository['name'], repository['revisionToFecth'])
        return

    revision_list = 'r' + ', r'.join(map(lambda commit: str(commit['revision']), pending_commits))
    print "Submitting revisions %s for %s to %s" % (revision_list, repository['name'], server_config['server']['url'])
    submit_commits(pending_commits, server_config['server']['url'], server_config['slave']['name'], server_config['slave']['password'])
    print "Successfully submitted."
    print


def determine_first_revision_to_fetch(dashboard_url, repository_name):
    try:
        last_reported_revision = fetch_revision_from_dasbhoard(dashboard_url, repository_name, 'last-reported')
    except Exception as error:
        sys.exit('Failed to fetch the latest reported commit: ' + str(error))

    if last_reported_revision:
        return last_reported_revision + 1

    # FIXME: This is a problematic if dashboard can get results for revisions older than oldest_revision
    # in the future because we never refetch older revisions.
    try:
        return fetch_revision_from_dasbhoard(dashboard_url, repository_name, 'oldest') or 1
    except Exception as error:
        sys.exit('Failed to fetch the oldest commit: ' + str(error))


def fetch_revision_from_dasbhoard(dashboard_url, repository_name, filter):
    result = urllib2.urlopen(dashboard_url + '/api/commits/' + repository_name + '/' + filter).read()
    parsed_result = json.loads(result)
    if parsed_result['status'] != 'OK' and parsed_result['status'] != 'RepositoryNotFound':
        raise Exception(result)
    commits = parsed_result.get('commits')
    return int(commits[0]['revision']) if commits else None


def fetch_commit_and_resolve_author(repository, account_to_name_helper, revision_to_fetch):
    try:
        commit = fetch_commit(repository, revision_to_fetch)
    except Exception as error:
        sys.exit('Failed to fetch the commit %d: %s' % (revision_to_fetch, str(error)))

    if not commit:
        return None

    account = commit['author']['account']
    try:
        name = resolve_author_name_from_account(account_to_name_helper, account) if account_to_name_helper else None
        if name:
            commit['author']['name'] = name
    except Exception as error:
        sys.exit('Failed to resolve the author name from an account %s: %s' % (account, str(error)))

    return commit


def fetch_commit(repository, revision):
    args = ['svn', 'log', '--revision', str(revision), '--xml', repository['url'], '--non-interactive']
    if 'username' in repository and 'password' in repository:
        args += ['--no-auth-cache', '--username', repository['username'], '--password', repository['password']]
    if repository.get('trustCertificate', False):
        args += ['--trust-server-cert']

    try:
        output = subprocess.check_output(args, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as error:
        if (': No such revision ' + str(revision)) in error.output:
            return None
        raise error
    xml = parseXmlString(output)
    time = text_content(xml.getElementsByTagName("date")[0])
    author_account = text_content(xml.getElementsByTagName("author")[0])
    message = text_content(xml.getElementsByTagName("msg")[0])
    return {
        'repository': repository['name'],
        'revision': revision,
        'time': time,
        'author': {'account': author_account},
        'message': message,
    }


name_account_compound_regex = re.compile(r'^\s*(?P<name>(\".+\"|[^<]+?))\s*\<(?P<account>.+)\>\s*$')


def resolve_author_name_from_account(helper, account):
    output = subprocess.check_output(helper + [account])
    match = name_account_compound_regex.match(output)
    if match:
        return match.group('name').strip('"')
    return output.strip()


if __name__ == "__main__":
    main(sys.argv)
