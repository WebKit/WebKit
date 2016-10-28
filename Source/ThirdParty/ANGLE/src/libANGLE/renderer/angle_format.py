#!/usr/bin/python
# Copyright 2016 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# angle_format.py:
#  Utils for ANGLE formats.

import json
import os

def reject_duplicate_keys(pairs):
    found_keys = {}
    for key, value in pairs:
        if key in found_keys:
           raise ValueError("duplicate key: %r" % (key,))
        else:
           found_keys[key] = value
    return found_keys

def load_json(path):
    with open(path) as map_file:
        file_data = map_file.read()
        map_file.close()
        return json.loads(file_data, object_pairs_hook=reject_duplicate_keys)

def load_forward_table(path):
    pairs = load_json(path)
    reject_duplicate_keys(pairs)
    return { gl: angle for gl, angle in pairs }

def load_inverse_table(path):
    pairs = load_json(path)
    reject_duplicate_keys(pairs)
    return { angle: gl for gl, angle in pairs }

def load_with_override(override_path):
    map_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'angle_format_map.json')
    results = load_forward_table(map_path)
    overrides = load_json(override_path)

    for k, v in overrides.iteritems():
        results[k] = v

    return results
