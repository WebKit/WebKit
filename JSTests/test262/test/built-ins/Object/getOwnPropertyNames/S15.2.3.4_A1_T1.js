// Copyright 2011 Google, Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Object.getOwnProperties and Object.prototype.hasOwnProperty should
    agree on what the own properties are.
es5id: 15.2.3.4_A1_T1
description: >
    Check that all the own property names reported by
    Object.getOwnPropertyNames on a strict function are names that
    hasOwnProperty agrees are own properties.
---*/

function foo() {}

var names = Object.getOwnPropertyNames(foo);
for (var i = 0, len = names.length; i < len; i++) {
  if (!foo.hasOwnProperty(names[i])) {
    throw new Test262Error('Phantom own property: ' + names[i]);
  }
}
