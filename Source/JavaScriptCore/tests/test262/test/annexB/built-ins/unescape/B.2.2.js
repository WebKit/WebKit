// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: B.2.2
description: >
    Object.getOwnPropertyDescriptor returns data desc for functions on
    built-ins (Global.unescape)
includes: [propertyHelper.js]
---*/

verifyWritable(this, "unescape");
verifyNotEnumerable(this, "unescape");
verifyConfigurable(this, "unescape");
