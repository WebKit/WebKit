#!/usr/bin/env python3

import base64
import json
import os
import sys

module = base64.b64decode(
    'AGFzbQEAAAABBwFgAn9/AX8DAgEABQMBAAEHEAIDYWRkAAAGbWVtb3J5AgAKCQEHACAAIAFqCwAaBG5hbWUAExJlbWJlZGRlZC1uYW1lLndhc20='
)


def encode_uleb128(value):
    result = bytearray()
    while True:
        byte = value & 0x7f
        value >>= 7
        result.append(byte | (0x80 if value else 0))
        if not value:
            return result


def append_string_custom_section(data, name, value):
    name = name.encode()
    value = value.encode()
    payload = encode_uleb128(len(name)) + name + encode_uleb128(len(value)) + value
    return data + b'\x00' + encode_uleb128(len(payload)) + payload


if os.environ.get("QUERY_STRING", "") == "source-map":
    source_map = {
        "version": 3,
        "file": "add.wasm",
        "sources": ["add.wat"],
        "sourcesContent": [
            '(module\n'
            '  (func (export "add") (param i32 i32) (result i32)\n'
            '    local.get 0\n'
            '    local.get 1\n'
            '    i32.add))\n'
        ],
        "names": [],
        "mappings": "iDAEA,EACA,EACA",
    }
    source_map_url = "data:application/json;base64," + base64.b64encode(json.dumps(source_map).encode()).decode()
    module = append_string_custom_section(module, "sourceMappingURL", source_map_url)

headers = b'Content-Type: application/wasm\r\n'
if os.environ.get('QUERY_STRING') == 'memory-cache':
    headers += b'Cache-Control: max-age=3600\r\n'

sys.stdout.buffer.write(headers + b'\r\n' + module)
