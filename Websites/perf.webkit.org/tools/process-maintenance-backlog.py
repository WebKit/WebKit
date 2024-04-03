#!/usr/bin/env python3

import argparse
import json
import os
import sys
from urllib.request import Request, urlopen

from util import load_server_config


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server-config-json', required=True, help='The path to a JSON file that specifies the perf dashboard.')
    args = parser.parse_args()

    maintenance_dir = determine_maintenance_dir()

    server_config = load_server_config(args.server_config_json)

    print(f'Submitting results in "{maintenance_dir}" to "{server_config["server"]["url"]}"')

    for filename in os.listdir(maintenance_dir):
        path = os.path.join(maintenance_dir, filename)
        if not os.path.isfile(path) or not filename.endswith('.json'):
            continue

        with open(os.path.join(maintenance_dir, path)) as submitted_json_file:
            submitted_content = submitted_json_file.read()

        print(f'{filename}...')
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

        print('Done')


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
        action = input(question + ' (y/n): ')
        if action == 'y' or action == 'yes':
            return True
        elif action == 'n' or action == 'no':
            return False
        else:
            print('Please answer by yes or no')


def submit_report(server_config, payload):
    try:
        request = Request(server_config['server']['url'] + '/api/report')
        request.add_header('Content-Type', 'application/json')
        request.add_header('Content-Length', len(payload))

        output = urlopen(request, payload).read()
        try:
            result = json.loads(output)
        except Exception as error:
            raise Exception(error, output)

        if result.get('status') != 'OK':
            raise Exception(result)

        return True
    except Exception as error:
        print(f'Failed to submit commits: {error}', file=sys.stderr)
        return False


if __name__ == "__main__":
    main()
