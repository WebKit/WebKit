// Copyright (C) 2016 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Object.getOwnPropertyDescriptors should be writable, non-enumerable, and configurable
esid: sec-object.getownpropertydescriptors
author: Jordan Harband
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Object, 'getOwnPropertyDescriptors');
verifyWritable(Object, 'getOwnPropertyDescriptors');
verifyConfigurable(Object, 'getOwnPropertyDescriptors');
