// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-parse-json-module
description: Creates extensible objects
flags: [module]
includes: [propertyHelper.js]
features: [import-assertions, json-modules]
---*/

import value from './json-value-object_FIXTURE.json' assert { type: 'json' };

value.test262property = 'test262 value';

verifyProperty(value, 'test262property', {
  value: 'test262 value',
  writable: true,
  enumerable: true,
  configurable: true
});
