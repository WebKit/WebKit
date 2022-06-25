// Copyright (C) 2015 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.values
description: Object.values should be writable, non-enumerable, and configurable
author: Jordan Harband
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Object, 'values');
verifyWritable(Object, 'values');
verifyConfigurable(Object, 'values');
