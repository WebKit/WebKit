// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-set.prototype.entries
description: >
    Set.prototype.entries ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.entries.name, "entries", "The value of `Set.prototype.entries.name` is `'entries'`");

verifyNotEnumerable(Set.prototype.entries, "name");
verifyNotWritable(Set.prototype.entries, "name");
verifyConfigurable(Set.prototype.entries, "name");
