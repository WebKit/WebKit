// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-weakset-iterable
description: >
  WeakSet ( [ iterable ] )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(this, 'WeakSet');
verifyWritable(this, 'WeakSet');
verifyConfigurable(this, 'WeakSet');
