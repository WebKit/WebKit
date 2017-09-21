// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-regexp.prototype.source
es5id: 15.10.7.1-2
es6id: 21.2.5.10
description: >
    RegExp.prototype.source is an accessor property whose set accessor
    function is undefined
includes: [propertyHelper.js]
---*/

var d = Object.getOwnPropertyDescriptor(RegExp.prototype, 'source');

assert.sameValue(typeof d.get, 'function', 'typeof d.get');
assert.sameValue(d.set, undefined, 'd.set');

verifyProperty(RegExp.prototype, 'source', {
  enumerable: false,
  configurable: true,
});
