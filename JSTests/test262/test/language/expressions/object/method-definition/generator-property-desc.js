// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Generator functions declared as methods are defined as enumerable,
    writable, configurable properties on the initialized object.
es6id: 14.4.13
includes: [propertyHelper.js]
features: [generators]
---*/

var obj = { *method() {} };

verifyEnumerable(obj, 'method');
verifyWritable(obj, 'method');
verifyConfigurable(obj, 'method');
