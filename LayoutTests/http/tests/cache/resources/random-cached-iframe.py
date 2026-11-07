#!/usr/bin/env python3

import sys
from random import randint

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: max-age=60000\r\n'
    'ETag: "123456789"\r\n\r\n'
    f'''
<script>
var randomNumber =  {randint(0, 999999999999)};
top.postMessage(randomNumber, "*");
</script>

'''
)
