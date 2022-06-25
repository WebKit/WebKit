// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.5-4-297
description: >
    Object.create - 'set' property of one property in 'Properties' is
    an Date object (8.10.5 step 8.b)
---*/


assert.throws(TypeError, function() {
  Object.create({}, {
    prop: {
      set: new Date()
    }
  });
});
