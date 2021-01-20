// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-ecmascript-standard-built-in-objects
description: Object.prototype.toString is not a constructor
info: |
  Built-in function objects that are not identified as constructors do
  not implement the [[Construct]] internal method unless otherwise specified
  in the description of a particular function
---*/

assert.throws(TypeError, function() {
  new Object.prototype.toString();
});
