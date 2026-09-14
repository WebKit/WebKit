#!/usr/bin/env python3

import http.cookies
import os
import sys
import urllib.parse


query = urllib.parse.parse_qs(os.environ.get("QUERY_STRING", ""))
token = query.get("token", [""])[0]
cookies = http.cookies.SimpleCookie(os.environ.get("HTTP_COOKIE", ""))
mode = cookies.get("modulepreload-reload-after-failure")
should_succeed = mode and mode.value == f"{token}-pass"
body = "export default 1;\n" if should_succeed else ""

if not should_succeed:
    sys.stdout.write("Status: 404 Not Found\r\n")
sys.stdout.write("Content-Type: text/javascript\r\n")
sys.stdout.write("Cache-Control: no-store\r\n")
sys.stdout.write(f"Content-Length: {len(body)}\r\n")
sys.stdout.write("\r\n")
sys.stdout.flush()

sys.stdout.write(body)
sys.stdout.flush()
