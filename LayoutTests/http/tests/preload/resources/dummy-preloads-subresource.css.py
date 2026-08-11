#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Link: <../resources/dummy.js>; rel=preload; as=script\r\n'
    'Link: <../resources/dummy.js?media>; rel=preload; as=script; media=all\r\n'
    'Content-Type: text/html\r\n\r\n'
    '/* This is just a dummy, empty CSS file */\n'
)