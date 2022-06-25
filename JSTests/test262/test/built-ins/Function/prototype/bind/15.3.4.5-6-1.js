// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5-6-1
description: Function.prototype.bind - F can get own data property
---*/

var foo = function() {};

var obj = foo.bind({});
obj.property = 12;

assert.sameValue(obj.property, 12, 'obj.property');
