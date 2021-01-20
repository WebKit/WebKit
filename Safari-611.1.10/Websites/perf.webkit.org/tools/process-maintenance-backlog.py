#!/usr/bin/python

import argparse
import json
import os
import sys
import urllib2

from util import load_server_config


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server-config-json', required=True, help='The path to a JSON file that specifies the perf dashboard.')
    args = parser.parse_args()

    maintenace_dir = determine_maintenance_dir()

    server_config = load_server_config(args.server_config_json)

    print 'Submitting results in "%s" to "%s"' % (maintenace_dir, server_config['server']['url'])

    for filename in os.listdir(maintenace_dir):
        path = os.path.join(maintenace_dir, filename)
        if os.path.isfile(path) and filename.endswith('.json'):

            with open(os.path.join(maintenace_dir, path)) as submitted_json_file:
                submitted_content = submitted_json_file.read()

            print '%s...' % filename,
            sys.stdout.flush()

            suffix = '.done'
            while True:
                if submit_report(server_config, submitted_content):
                    break
                if ask_yes_no_question('Suffix the file with .error and continue?'):
                    suffix = '.error'
                    break
                else:
                    sys.exit(0)

            os.rename(path, path + suffix)

            print 'Done'


def determine_maintenance_dir():
    root_dir = os.path.join(os.path.dirname(__file__), '../')

    config_json_path = os.path.abspath(os.path.join(root_dir, 'config.json'))
    with open(config_json_path) as config_json_file:
        config = json.load(config_json_file)

    dirname = config.get('maintenanceDirectory')
    if not dirname:
        sys.exit('maintenanceDirectory is not specified in config.json')

    return os.path.abspath(os.path.join(root_dir, dirname))


def ask_yes_no_question(question):
    while True:
        action = raw_input(question + ' (y/n): ')
        if action == 'y' or action == 'yes':
            return True
        elif action == 'n' or action == 'no':
            return False
        else:
            print 'Please answer by yes or no'


def submit_report(server_config, payload):
    try:
        request = urllib2.Request(server_config['server']['url'] + '/api/report')
        request.add_header('Content-Type', 'application/json')
        request.add_header('Content-Length', len(payload))

        output = urllib2.urlopen(request, payload).read()
        try:
            result = json.loads(output)
        except Exception, error:
            raise Exception(error, output)

        if result.get('status') != 'OK':
            raise Exception(result)

        return True
    except Exception as error:
        print >> sys.stderr, 'Failed to submit commits: %s' % str(error)
        return False


if __name__ == "__main__":
    main()
