# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PYTHON_VERSION_COMPATIBILITY = "PY3"

DEPS = [
  'depot_tools/gclient',
  'docker',
  'env',
  'infra',
  'recipe_engine/context',
  'recipe_engine/file',
  'recipe_engine/path',
  'recipe_engine/step',
  'run',
  'vars',
]

from recipe_engine.recipe_api import Property

PROPERTIES = {
  'buildername': Property(default=None),
}
