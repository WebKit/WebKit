// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.13
description: >
  Reflect.set is configurable, writable and not enumerable.
info: |
  26.1.13 Reflect.set ( target, propertyKey, V [ , receiver ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
features: [Reflect]
---*/

verifyNotEnumerable(Reflect, 'set');
verifyWritable(Reflect, 'set');
verifyConfigurable(Reflect, 'set');
