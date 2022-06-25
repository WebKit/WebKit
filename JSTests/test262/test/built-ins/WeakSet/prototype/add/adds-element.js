// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-weakset.prototype.add
description: >
  Appends value as the last element of entries.
info: |
  WeakSet.prototype.add ( value )

  ...
  7. Append value as the last element of entries.
  ...
---*/

var s = new WeakSet();
var foo = {};
var bar = {};
var baz = {};

s.add(foo);
s.add(bar);
s.add(baz);

assert(s.has(foo));
assert(s.has(bar));
assert(s.has(baz));
