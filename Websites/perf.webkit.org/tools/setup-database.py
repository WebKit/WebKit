#!/usr/bin/python

import json
import os
import shutil
import subprocess
import sys
import time


def main():
    if len(sys.argv) <= 1:
        sys.exit('Specify the database directory')

    database_dir = os.path.abspath(sys.argv[1])
    if os.path.exists(database_dir) and not os.path.isdir(database_dir):
        sys.exit('The specified path is not a directory')

    database_config = load_database_config()

    database_name = database_config['name']
    username = database_config['username']
    password = database_config['password']

    psql_dir = determine_psql_dir()

    print "Initializing database at %s" % database_dir
    execute_psql_command(psql_dir, 'initdb', [database_dir])

    print "Starting postgres at %s" % database_dir
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
    print 'Postgres is running!'

    done = False
    try:
        print "Creating database: %s" % database_name
        execute_psql_command(psql_dir, 'createdb', [database_name, '--host', 'localhost'])

        print "Creating user: %s" % username
        execute_psql_command(psql_dir, 'psql', [database_name, '--host', 'localhost', '--command',
            "create role \"%s\" with nosuperuser login encrypted password '%s';" % (username, password)])

        print "Granting all access on %s to %s" % (database_name, username)
        execute_psql_command(psql_dir, 'psql', [database_name, '--host', 'localhost', '--command',
            'grant all privileges on database "%s" to "%s";' % (database_name, username)])

        print "Successfully configured postgres database %s at %s" % (database_name, database_dir)

        done = True
    except Exception as exception:
        start_or_stop_database(psql_dir, database_dir, 'stop')
        shutil.rmtree(database_dir)
        raise exception

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


if __name__ == "__main__":
    main()
