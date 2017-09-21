// Copyright (C) 2017 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.sort
description: throws on a non-undefined non-function
info: >
  22.1.3.25 Array.prototype.sort ( comparefn )

  Upon entry, the following steps are performed to initialize evaluation
  of the sort function:

  ...
  1. If _comparefn_ is not *undefined* and IsCallable(_comparefn_) is *false*, throw a *TypeError* exception.
  ...
features: [Symbol]
---*/

var sample = [1, 2, 3];

assert.throws(TypeError, function() {
 sample.sort(null);
});

assert.throws(TypeError, function() {
  sample.sort(true);
});

assert.throws(TypeError, function() {
  sample.sort(false);
});

assert.throws(TypeError, function() {
  sample.sort('');
});

assert.throws(TypeError, function() {
  sample.sort(/a/g);
});

assert.throws(TypeError, function() {
  sample.sort(42);
});

assert.throws(TypeError, function() {
  sample.sort([]);
});

assert.throws(TypeError, function() {
  sample.sort({});
});

assert.throws(TypeError, function() {
  sample.sort(Symbol());
});
