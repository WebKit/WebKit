#!/usr/bin/python

import json

with open('config.json', 'r') as config_file:
    config = json.load(config_file)
    passwords = {}
    for slave in config['slaves']:
        passwords[slave['name']] = 'a'

with open('passwords.json', 'w') as passwords_file:
    passwords_file.write(json.dumps(passwords))
