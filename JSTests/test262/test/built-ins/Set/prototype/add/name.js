// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-set.prototype.add
description: >
    Set.prototype.add ( value )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.add.name, "add", "The value of `Set.prototype.add.name` is `'add'`");

verifyNotEnumerable(Set.prototype.add, "name");
verifyNotWritable(Set.prototype.add, "name");
verifyConfigurable(Set.prototype.add, "name");
