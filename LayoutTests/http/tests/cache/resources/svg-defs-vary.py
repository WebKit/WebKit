#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: image/svg+xml\r\n'
    'Vary: Cookie\r\n'
    'Cache-Control: max-age=31536000\r\n\r\n'
    '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">\n'
    '<defs>\n'
    '<g id="circle">\n'
    '<circle style="fill:green" r="10"/>\n'
    '</g>\n'
    '</defs>\n'
    '</svg>\n'
)