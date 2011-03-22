#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Put an object file on the sources list.
Expect the result to link ok.
"""

import TestGyp

# Currently only works under the make build.
test = TestGyp.TestGyp(formats=['make'])

test.run_gyp('link-objects.gyp')

test.build('link-objects.gyp', test.ALL)

test.run_built_executable('link-objects', stdout="PASS\n")

test.up_to_date('link-objects.gyp', test.ALL)

test.pass_test()
