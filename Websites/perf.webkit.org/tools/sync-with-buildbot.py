#!/usr/bin/python

import argparse
import base64
import copy
import json
import sys
import time
import urllib
import urllib2

from util import load_server_config


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--triggerable', required=True, help='The name of the triggerable to process. e.g. build-webkit')
    parser.add_argument('--buildbot-url', required=True, help='URL for a buildbot builder; e.g. "https://build.webkit.org/"')
    parser.add_argument('--builder-config-json', required=True, help='The path to a JSON file that specifies which test and platform will be posted to which builder. '
        'The JSON should contain an array of dictionaries with keys "platform", "test", and "builder" '
        'with the platform name (e.g. mountainlion), the test path (e.g. ["Parser", "html5-full-render"]), and the builder name (e.g. Apple MountainLion Release (Perf)) as values.')
    parser.add_argument('--server-config-json', required=True, help='The path to a JSON file that specifies the perf dashboard.')

    parser.add_argument('--lookback-count', type=int, default=10, help='The number of builds to look back when finding in-progress builds on the buildbot')
    parser.add_argument('--seconds-to-sleep', type=float, default=120, help='The seconds to sleep between iterations')
    args = parser.parse_args()

    configurations = load_config(args.builder_config_json, args.buildbot_url.strip('/'))

    request_updates = {}
    while True:
        server_config = load_server_config(args.server_config_json)
        request_updates.update(find_request_updates(configurations, args.lookback_count))
        if request_updates:
            print 'Updating the build requests %s...' % ', '.join(map(str, request_updates.keys()))
        else:
            print 'No updates...'

        payload = {
            'buildRequestUpdates': request_updates,
            'slaveName': server_config['slave']['name'],
            'slavePassword': server_config['slave']['password']}

        build_requests_url = server_config['server']['url'] + '/api/build-requests/' + args.triggerable + '?useLegacyIdResolution=true'
        response = update_and_fetch_build_requests(build_requests_url, payload)
        open_requests = response.get('buildRequests', [])

        root_sets = organize_root_sets_by_id_and_repository_names(response.get('rootSets', {}), response.get('roots', []))

        for request in filter(lambda request: request['status'] == 'pending', open_requests):
            config = config_for_request(configurations, request)
            if not config:
                print >> sys.stderr, "Failed to find the configuration for request %s: %s" % (str(request['id']), json.dumps(request))
                continue
            if config and len(config['scheduledRequests']) < 1:
                print "Scheduling the build request %s..." % str(request['id'])
                schedule_request(config, request, root_sets)

        request_updates = find_stale_request_updates(configurations, open_requests, request_updates.keys())
        if request_updates:
            print "Found stale build requests %s..." % ', '.join(map(str, request_updates.keys()))

        time.sleep(args.seconds_to_sleep)


def load_config(config_json_path, buildbot_url):
    with open(config_json_path) as config_json:
        options = json.load(config_json)

    shared_config = options['shared']
    type_config = options['types']
    builder_config = options['builders']

    def merge(config, config_to_merge):
        for key, value in config_to_merge.iteritems():
            if isinstance(value, dict):
                config.setdefault(key, {})
                config[key].update(value)
            else:
                config[key] = value

    scheduled_requests_by_builder = {}

    configurations = options['configurations']
    for config in configurations:

        merge(config, shared_config)
        merge(config, type_config[config.pop('type')])
        merge(config, builder_config[config.pop('builder')])

        escaped_builder_name = urllib.quote(config['builder'])
        config['url'] = '%s/builders/%s/' % (buildbot_url, escaped_builder_name)
        config['jsonURL'] = '%s/json/builders/%s/' % (buildbot_url, escaped_builder_name)

        scheduled_requests_by_builder.setdefault(config['builder'], set())
        config['scheduledRequests'] = scheduled_requests_by_builder[config['builder']]

    return configurations


def find_request_updates(configurations, lookback_count):
    request_updates = {}

    for config in configurations:
        config['scheduledRequests'].clear()

    for config in configurations:
        try:
            pending_builds = fetch_json(config['jsonURL'] + 'pendingBuilds')
            scheduled_requests = filter(None, [request_id_from_build(config, build) for build in pending_builds])
            for request_id in scheduled_requests:
                request_updates[request_id] = {'status': 'scheduled', 'url': config['url']}
                config['scheduledRequests'].add(request_id)
        except (IOError, ValueError) as error:
            print >> sys.stderr, "Failed to fetch pending builds for %s: %s" % (config['builder'], str(error))

    for config in configurations:
        for i in range(1, lookback_count + 1):
            build_error = None
            build_index = -i
            try:
                build = fetch_json(config['jsonURL'] + 'builds/%d' % build_index)
                request_id = request_id_from_build(config, build)
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


def update_and_fetch_build_requests(build_requests_url, payload):
    try:
        response = fetch_json(build_requests_url, payload=json.dumps(payload))
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


def organize_root_sets_by_id_and_repository_names(root_sets, roots):
    result = {}
    root_by_id = {}
    for root in roots:
        root_by_id[root['id']] = root

    for root_set in root_sets:
        roots_by_repository = {}
        for root_id in root_set['roots']:
            root = root_by_id[root_id]
            roots_by_repository[root['repository']] = root
        result[root_set['id']] = roots_by_repository

    return result


def schedule_request(config, request, root_sets):
    roots = root_sets[request['rootSet']]
    payload = {}
    for property_name, property_value in config['arguments'].iteritems():
        if not isinstance(property_value, dict):
            payload[property_name] = property_value
        elif 'root' in property_value:
            repository_name = property_value['root']
            if repository_name in roots:
                payload[property_name] = roots[repository_name]['revision']
        elif 'rootsExcluding' in property_value:
            excluded_roots = property_value['rootsExcluding']
            filtered_roots = {}
            for root_name in roots:
                if root_name not in excluded_roots:
                    filtered_roots[root_name] = roots[root_name]
            payload[property_name] = json.dumps(filtered_roots)
        else:
            print >> sys.stderr, "Failed to process an argument %s: %s" % (property_name, property_value)
            return
    payload[config['buildRequestArgument']] = request['id']

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


def fetch_json(url, payload=None):
    request = urllib2.Request(url)
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


def request_id_from_build(config, build):
    job_id = property_value_from_build(build, config['buildRequestArgument'])
    return int(job_id) if job_id and job_id.isdigit() else None


if __name__ == "__main__":
    main()
