// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-additional-properties-of-the-object.prototype-object
description: >
    Behavior when "this" value defines a like-named accessor property without a
    `set` method
info: |
    [...]
    4. Repeat
       a. Let desc be ? O.[[GetOwnProperty]](key).
       b. If desc is not undefined, then
          i. If IsAccessorDescriptor(desc) is true, return desc.[[Set]].
          ii. Return undefined.
       c. Let O be ? O.[[GetPrototypeOf]]().
       d. If O is null, return undefined.
features: [__setter__]
---*/

var root = Object.defineProperty({}, 'target', { set: function() {} });
var desc = { get: function() {} };
var subject = Object.create(root, { target: desc });

assert.sameValue(subject.__lookupSetter__('target'), undefined);
