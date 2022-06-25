#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
message = query.get('message', [''])[0]

cookies = {}
if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        cookies[cookie[0]] = cookie[1]

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '{}<br>'.format(message)
)

for name in [
    query.get('name1', [''])[0],
    query.get('name2', [None])[0],
    query.get('name3', [None])[0]
]:
    if not name:
        continue
    cookie = cookies.get(name, None)
    if not cookie:
        print('Did not receive cookie named \'{}\'.<br>'.format(name), end='')
    else:
        print('Received cookie named \'{}\'.<br>'.format(name), end='')

print('''<p id="output"></p>
<script>
    document.getElementById("output").textContent = "Client-side document.cookie: " + document.cookie;

    function messageToTop(messagePrefix, fetchData) {
        top.postMessage(messagePrefix + " document.cookie == " + document.cookie +
            (fetchData ? ", cookies seen server-side == " + JSON.stringify(fetchData) : ""), "http://127.0.0.1:8000");
    }

    function receiveMessage(event) {
        if (event.origin === "http://127.0.0.1:8000") {
            if (event.data.indexOf("reportBackCookies") !== -1) {
                fetch("echo-incoming-cookies-as-json.py", { credentials: "same-origin" }).then(function(response) {
                    return response.json();
                }).then(function(data) {
                    messageToTop("PASS", data);
                }).catch(function(error) {
                    console.log(error.message);
                });
            } else {
                messageToTop("FAIL Unknown request.");
            }
        } else {
            messageToTop("Fail Received a message from an unexpected origin: " + event.origin);
        }
    }

    window.addEventListener("message", receiveMessage, false);
</script>''')