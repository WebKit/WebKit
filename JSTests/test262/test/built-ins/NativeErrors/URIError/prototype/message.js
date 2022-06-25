// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.5.6.3.2
description: >
  The initial value of URIError.prototype.message is the empty string.
info: |
  The initial value of the message property of the prototype for a given NativeError
  constructor is the empty String.

  17 ECMAScript Standard Built-in Objects:
    Every other data property described in clauses 18 through 26 and in Annex B.2 has
    the attributes { [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true }
    unless otherwise specified.
includes: [propertyHelper.js]
---*/

assert.sameValue(URIError.prototype.message, "");

verifyNotEnumerable(URIError.prototype, "message");
verifyWritable(URIError.prototype, "message");
verifyConfigurable(URIError.prototype, "message");
