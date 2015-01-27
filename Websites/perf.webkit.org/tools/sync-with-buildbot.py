#!/usr/bin/python

import argparse
import base64
import copy
import json
import sys
import time
import urllib
import urllib2


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-requests-url', required=True, help='URL for the build requests JSON API; e.g. https://perf.webkit.org/api/build-requests/build.webkit.org/')
    parser.add_argument('--build-requests-user', help='The username for Basic Authentication to access the build requests JSON API')
    parser.add_argument('--build-requests-password', help='The password for Basic Authentication to access the build requests JSON API')
    parser.add_argument('--slave-name', required=True, help='The slave name used to update the build requets status')
    parser.add_argument('--slave-password', required=True, help='The slave password used to update the build requets status')
    parser.add_argument('--buildbot-url', required=True, help='URL for a buildbot builder; e.g. "https://build.webkit.org/"')
    parser.add_argument('--builder-config-json', required=True, help='The path to a JSON file that specifies which test and platform will be posted to which builder. '
        'The JSON should contain an array of dictionaries with keys "platform", "test", and "builder" '
        'with the platform name (e.g. mountainlion), the test path (e.g. ["Parser", "html5-full-render"]), and the builder name (e.g. Apple MountainLion Release (Perf)) as values.')
    parser.add_argument('--lookback-count', type=int, default=10, help='The number of builds to look back when finding in-progress builds on the buildbot')
    parser.add_argument('--seconds-to-sleep', type=float, default=120, help='The seconds to sleep between iterations')
    args = parser.parse_args()

    configurations = load_config(args.builder_config_json, args.buildbot_url.strip('/'))
    build_request_auth = {'user': args.build_requests_user, 'password': args.build_requests_password or ''} if args.build_requests_user else None
    request_updates = {}
    while True:
        request_updates.update(find_request_updates(configurations, args.lookback_count))
        if request_updates:
            print 'Updating the build requests %s...' % ', '.join(map(str, request_updates.keys()))
        else:
            print 'No updates...'

        payload = {'buildRequestUpdates': request_updates, 'slaveName': args.slave_name, 'slavePassword': args.slave_password}
        response = update_and_fetch_build_requests(args.build_requests_url, build_request_auth, payload)
        root_sets = response.get('rootSets', {})
        open_requests = response.get('buildRequests', [])

        for request in filter(lambda request: request['status'] == 'pending', open_requests):
            config = config_for_request(configurations, request)
            if len(config['scheduledRequests']) < 1:
                print "Scheduling the build request %s..." % str(request['id'])
                schedule_request(config, request, root_sets)

        request_updates = find_stale_request_updates(configurations, open_requests, request_updates.keys())
        if request_updates:
            print "Found stale build requests %s..." % ', '.join(map(str, request_updates.keys()))

        time.sleep(args.seconds_to_sleep)


def load_config(config_json_path, buildbot_url):
    with open(config_json_path) as config_json:
        configurations = json.load(config_json)

    for config in configurations:
        escaped_builder_name = urllib.quote(config['builder'])
        config['url'] = '%s/builders/%s/' % (buildbot_url, escaped_builder_name)
        config['jsonURL'] = '%s/json/builders/%s/' % (buildbot_url, escaped_builder_name)
        config['scheduledRequests'] = set()

    return configurations


def find_request_updates(configurations, lookback_count):
    request_updates = {}

    for config in configurations:
        try:
            pending_builds = fetch_json(config['jsonURL'] + 'pendingBuilds')
            scheduled_requests = filter(None, [request_id_from_build(build) for build in pending_builds])
            for request_id in scheduled_requests:
                request_updates[request_id] = {'status': 'scheduled', 'url': config['url']}
            config['scheduledRequests'] = set(scheduled_requests)
        except (IOError, ValueError) as error:
            print >> sys.stderr, "Failed to fetch pending builds for %s: %s" % (config['builder'], str(error))

    for config in configurations:
        for i in range(1, lookback_count + 1):
            build_error = None
            build_index = -i
            try:
                build = fetch_json(config['jsonURL'] + 'builds/%d' % build_index)
                request_id = request_id_from_build(build)
                if not request_id:
                    continue

                in_progress = build.get('currentStep')
                if in_progress:
                    request_updates[request_id] = {'status': 'running', 'url': config['url']}
                    config['scheduledRequests'].discard(request_id)
                else:
                    url = config['url'] + 'builds/' + str(build['number'])
                    request_updates[request_id] = {'status': 'failedIfNotCompleted', 'url': url}
            except urllib2.HTTPError as error:
                if error.code == 404:
                    break
                else:
                    build_error = error
            except ValueError as error:
                build_error = error
            if build_error:
                print >> sys.stderr, "Failed to fetch build %d for %s: %s" % (build_index, config['builder'], str(build_error))

    return request_updates


def update_and_fetch_build_requests(build_requests_url, build_request_auth, payload):
    try:
        response = fetch_json(build_requests_url, payload=json.dumps(payload), auth=build_request_auth)
        if response['status'] != 'OK':
            raise ValueError(response['status'])
        return response
    except (IOError, ValueError) as error:
        print >> sys.stderr, 'Failed to update or fetch build requests at %s: %s' % (build_requests_url, str(error))
    return {}


def find_stale_request_updates(configurations, open_requests, requests_on_buildbot):
    request_updates = {}
    for request in open_requests:
        request_id = int(request['id'])
        should_be_on_buildbot = request['status'] in ('scheduled', 'running')
        if should_be_on_buildbot and request_id not in requests_on_buildbot:
            config = config_for_request(configurations, request)
            if config:
                request_updates[request_id] = {'status': 'failed', 'url': config['url']}
    return request_updates


def schedule_request(config, request, root_sets):
    replacements = root_sets.get(request['rootSet'], {})
    replacements['buildRequest'] = request['id']

    payload = {}
    for property_name, property_value in config['arguments'].iteritems():
        for key, value in replacements.iteritems():
            property_value = property_value.replace('$' + key, value)
        payload[property_name] = property_value

    try:
        urllib2.urlopen(urllib2.Request(config['url'] + 'force'), urllib.urlencode(payload))
        config['scheduledRequests'].add(request['id'])
    except (IOError, ValueError) as error:
        print >> sys.stderr, "Failed to fetch pending builds for %s: %s" % (config['builder'], str(error))


def config_for_request(configurations, request):
    for config in configurations:
        if config['platform'] == request['platform'] and config['test'] == request['test']:
            return config
    return None


def fetch_json(url, auth={}, payload=None):
    request = urllib2.Request(url)
    if auth:
        request.add_header('Authorization', "Basic %s" % base64.encodestring('%s:%s' % (auth['user'], auth['password'])).rstrip('\n'))
    response = urllib2.urlopen(request, payload).read()
    try:
        return json.loads(response)
    except ValueError as error:
        raise ValueError(str(error) + '\n' + response)


def property_value_from_build(build, name):
    for prop in build.get('properties', []):
        if prop[0] == name:
            return prop[1]
    return None


def request_id_from_build(build):
    job_id = property_value_from_build(build, 'jobid')
    return int(job_id) if job_id and job_id.isdigit() else None


if __name__ == "__main__":
    main()
