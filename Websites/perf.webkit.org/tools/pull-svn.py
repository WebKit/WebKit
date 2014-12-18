#!/usr/bin/python

import json
import re
import subprocess
import sys
import time
import urllib2

from xml.dom.minidom import parseString as parseXmlString


def main(argv):
    if len(argv) < 7:
        sys.exit('Usage: pull-svn <repository-name> <repository-URL> <dashboard-URL> <builder-name> <builder-password> <seconds-to-sleep> [<account-to-name-helper>]')

    repository_name = argv[1]
    repository_url = argv[2]
    dashboard_url = argv[3]
    builder_name = argv[4]
    builder_password = argv[5]
    seconds_to_sleep = float(argv[6])
    account_to_name_helper = argv[7] if len(argv) > 7 else None

    print "Submitting revision logs for %s at %s to %s" % (repository_name, repository_url, dashboard_url)

    revision_to_fetch = determine_first_revision_to_fetch(dashboard_url, repository_name)
    print "Start fetching commits at r%d" % revision_to_fetch

    pending_commits_to_send = []

    while True:
        commit = fetch_commit_and_resolve_author(repository_name, repository_url, account_to_name_helper, revision_to_fetch)

        if commit:
            print "Fetched r%d." % revision_to_fetch
            pending_commits_to_send += [commit]
            revision_to_fetch += 1
        else:
            print "Revision %d not found" % revision_to_fetch

        if not commit or len(pending_commits_to_send) >= 10:
            if pending_commits_to_send:
                print "Submitting the above commits to %s..." % dashboard_url
                submit_commits(pending_commits_to_send, dashboard_url, builder_name, builder_password)
                print "Successfully submitted."
            pending_commits_to_send = []
            time.sleep(seconds_to_sleep)


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


def fetch_commit_and_resolve_author(repository_name, repository_url, account_to_name_helper, revision_to_fetch):
    try:
        commit = fetch_commit(repository_name, repository_url, revision_to_fetch)
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


def fetch_commit(repository_name, repository_url, revision):
    args = ['svn', 'log', '--revision', str(revision), '--xml', repository_url]
    try:
        output = subprocess.check_output(args, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as error:
        if (': No such revision ' + str(revision)) in error.output:
            return None
        raise error
    xml = parseXmlString(output)
    time = textContent(xml.getElementsByTagName("date")[0])
    author_account = textContent(xml.getElementsByTagName("author")[0])
    message = textContent(xml.getElementsByTagName("msg")[0])
    return {
        'repository': repository_name,
        'revision': revision,
        'time': time,
        'author': {'account': author_account},
        'message': message,
    }


def textContent(element):
    text = ''
    for child in element.childNodes:
        if child.nodeType == child.TEXT_NODE:
            text += child.data
        else:
            text += textContent(child)
    return text


name_account_compound_regex = re.compile(r'^\s*(?P<name>(\".+\"|[^<]+?))\s*\<(?P<account>.+)\>\s*$')


def resolve_author_name_from_account(helper, account):
    output = subprocess.check_output(helper + ' ' + account, shell=True)
    match = name_account_compound_regex.match(output)
    if match:
        return match.group('name').strip('"')
    return output.strip()


def submit_commits(commits, dashboard_url, builder_name, builder_password):
    try:
        payload = json.dumps({
            'builderName': builder_name,
            'builderPassword': builder_password,
            'commits': commits,
        })
        request = urllib2.Request(dashboard_url + '/api/report-commits')
        request.add_header('Content-Type', 'application/json')
        request.add_header('Content-Length', len(payload))

        output = urllib2.urlopen(request, payload).read()
        try:
            result = json.loads(output)
        except Exception, error:
            raise Exception(error, output)

        if result.get('status') != 'OK':
            raise Exception(result)
    except Exception as error:
        sys.exit('Failed to submit commits: %s' % str(error))


if __name__ == "__main__":
    main(sys.argv)
