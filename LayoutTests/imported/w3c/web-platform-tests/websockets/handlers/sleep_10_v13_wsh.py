#!/usr/bin/python

import sys, urllib, time
from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
    request.connection.write(b'H')
    time.sleep(2)
    request.connection.write(b'T')
    time.sleep(2)
    request.connection.write(b'T')
    time.sleep(2)
    request.connection.write(b'P')
    time.sleep(2)
    request.connection.write(b'/')
    time.sleep(2)
    return

def web_socket_transfer_data(request):
    while True:
        line = msgutil.receive_message(request)
        if line == 'Goodbye':
            return
        request.ws_stream.send_message(line, binary=False)
