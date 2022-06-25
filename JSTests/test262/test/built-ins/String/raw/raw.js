// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.4
description: >
  String.raw property descriptor
info: |
  String.raw ( template , ...substitutions )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyNotEnumerable(String, 'raw');
verifyWritable(String, 'raw');
verifyConfigurable(String, 'raw');
