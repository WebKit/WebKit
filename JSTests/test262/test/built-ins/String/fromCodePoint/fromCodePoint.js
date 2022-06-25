// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.2
description: >
  String.fromCodePoint property descriptor
info: |
  String.fromCodePoint ( ...codePoints )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [String.fromCodePoint]
---*/

verifyNotEnumerable(String, 'fromCodePoint');
verifyWritable(String, 'fromCodePoint');
verifyConfigurable(String, 'fromCodePoint');
