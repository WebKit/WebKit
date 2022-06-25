// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.5.1.1
description: >
  A new instance has the message property if created with a parameter
info: |
  19.5.1.1 Error ( message )

  ...
  4. If message is not undefined, then
    a. Let msg be ToString(message).
    b. ReturnIfAbrupt(msg).
    c. Let msgDesc be the PropertyDescriptor{[[Value]]: msg, [[Writable]]: true,
    [[Enumerable]]: false, [[Configurable]]: true}.
    d. Let status be DefinePropertyOrThrow(O, "message", msgDesc).
  ...
includes: [propertyHelper.js]

---*/

class Err extends Error {}

Err.prototype.message = 'custom-error';

var err1 = new Err('foo 42');
assert.sameValue(err1.message, 'foo 42');
assert(err1.hasOwnProperty('message'));

verifyWritable(err1, 'message');
verifyNotEnumerable(err1, 'message');
verifyConfigurable(err1, 'message');

var err2 = new Err();
assert.sameValue(err2.hasOwnProperty('message'), false);
assert.sameValue(err2.message, 'custom-error');
