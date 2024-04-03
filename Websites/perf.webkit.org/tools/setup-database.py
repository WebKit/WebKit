#!/usr/bin/env python3

import argparse
import json
import os
import shutil
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('database_directory')
    parser.add_argument('--keep-database-running', default=False, action='store_true',
                        help='Keep database running after setup')
    args = parser.parse_args()

    database_dir = os.path.abspath(args.database_directory)
    if os.path.exists(database_dir) and not os.path.isdir(database_dir):
        sys.exit('The specified path is not a directory')

    database_config = load_database_config()

    database_name = database_config['name']
    username = database_config['username']
    password = database_config['password']

    psql_dir = determine_psql_dir()

    print(f'Initializing database at {database_dir}')
    execute_psql_command(psql_dir, 'initdb', [database_dir])

    print(f'Starting postgres at {database_dir}')
    start_or_stop_database(psql_dir, database_dir, 'start')
    postgres_is_running = False
    for unused in range(0, 5):
        try:
            execute_psql_command(psql_dir, 'psql', ['--list', '--host', 'localhost'], suppressStdout=True)
            postgres_is_running = True
        except subprocess.CalledProcessError:
            time.sleep(0.5)

    if not postgres_is_running:
        sys.exit('Postgres failed to start in time')
    print('Postgres is running!')

    keep_database_running = args.keep_database_running
    try:
        print(f'Creating database: {database_name}')
        execute_psql_command(psql_dir, 'createdb', [database_name, '--host', 'localhost'])

        print(f'Creating user: {username}')
        execute_psql_command(psql_dir, 'psql', [database_name, '--host', 'localhost', '--command',
                                                f'create role "{username}" with nosuperuser login '
                                                f'encrypted password \'{password}\';'])

        print(f'Granting all access on {database_name} to {username}')
        execute_psql_command(psql_dir, 'psql', [database_name, '--host', 'localhost', '--command',
                                                f'grant all privileges on database "{database_name}" to "{username}";'])
        # PostgreSQL 15 revokes the CREATE permission from all users except a database owner from the public
        # (or default) schema
        execute_psql_command(psql_dir, 'psql', [database_name, '--host', 'localhost', '--command',
                                                f'grant all on schema public to "{username}";'])

        print(f'Successfully configured postgres database {database_name} at {database_dir}')

    except Exception as exception:
        keep_database_running = False
        shutil.rmtree(database_dir)
        raise exception
    finally:
        if not keep_database_running:
            start_or_stop_database(psql_dir, database_dir, 'stop')

    sys.exit(0)


def load_database_config():
    config_json_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../config.json'))
    with open(config_json_path) as config_json_file:
        database_config = json.load(config_json_file).get('database')

    if database_config.get('host') != 'localhost':
        sys.exit('Cannot setup a database on a remote machine')

    return database_config


def determine_psql_dir():
    psql_dir = ''
    try:
        subprocess.check_output(['initdb', '--version'])
    except OSError:
        psql_dir = '/Applications/Server.app/Contents/ServerRoot/usr/bin/'
        try:
            subprocess.check_output([psql_dir + 'initdb', '--version'])
        except OSError:
            sys.exit('Cannot find psql')
    return psql_dir


def start_or_stop_database(psql_dir, database_dir, command):
    execute_psql_command(psql_dir, 'pg_ctl', ['-D', database_dir,
        '-l', os.path.join(database_dir, 'logfile'),
        '-o', '-k ' + database_dir,
        command])


def execute_psql_command(psql_dir, command, args=[], suppressStdout=False):
    return subprocess.check_output([os.path.join(psql_dir, command)] + args,
        stderr=subprocess.STDOUT if suppressStdout else None)


if __name__ == '__main__':
    main()
