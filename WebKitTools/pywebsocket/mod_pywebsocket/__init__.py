# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Web Socket extension for Apache HTTP Server.

mod_pywebsocket is a Web Socket extension for Apache HTTP Server
intended for testing or experimental purposes. mod_python is required.

Installation:

0. Prepare an Apache HTTP Server for which mod_python is enabled.

1. Specify the following Apache HTTP Server directives to suit your
   configuration.

   If mod_pywebsocket is not in the Python path, specify the following.
   <websock_lib> is the directory where mod_pywebsocket is installed.

       PythonPath "sys.path+['<websock_lib>']"

   Always specify the following. <websock_handlers> is the directory where
   user-written Web Socket handlers are placed.

       PythonOption mod_pywebsocket.handler_root <websock_handlers>
       PythonHeaderParserHandler mod_pywebsocket.headerparserhandler

   To limit the search for Web Socket handlers to a directory <scan_dir>
   under <websock_handlers>, configure as follows:
   
       PythonOption mod_pywebsocket.handler_scan <scan_dir>
       
   <scan_dir> is useful in saving scan time when <websock_handlers>
   contains many non-Web Socket handler files.

   Example snippet of httpd.conf:
   (mod_pywebsocket is in /websock_lib, Web Socket handlers are in
   /websock_handlers, port is 80 for ws, 443 for wss.)

       <IfModule python_module>
         PythonPath "sys.path+['/websock_lib']"
         PythonOption mod_pywebsocket.handler_root /websock_handlers
         PythonHeaderParserHandler mod_pywebsocket.headerparserhandler
       </IfModule>

Writing Web Socket handlers:

When a Web Socket request comes in, the resource name
specified in the handshake is considered as if it is a file path under
<websock_handlers> and the handler defined in
<websock_handlers>/<resource_name>_wsh.py is invoked.

For example, if the resource name is /example/chat, the handler defined in
<websock_handlers>/example/chat_wsh.py is invoked.

A Web Socket handler is composed of the following two functions:

    web_socket_do_extra_handshake(request)
    web_socket_transfer_data(request)

where:
    request: mod_python request.

web_socket_do_extra_handshake is called during the handshake after the
headers are successfully parsed and Web Socket properties (ws_location,
ws_origin, ws_protocol, and ws_resource) are added to request. A handler
can reject the request by raising an exception.

web_socket_transfer_data is called after the handshake completed
successfully. A handler can receive/send messages from/to the client
using request. mod_pywebsocket.msgutil module provides utilities
for data transfer.
"""


# vi:sts=4 sw=4 et tw=72
