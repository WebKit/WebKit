// Copyright (c) 2021 Jamie Kyle.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.hasown
description: Testing descriptor property of Object.hasOwn
includes:
    - propertyHelper.js
author: Jamie Kyle
features: [Object.hasOwn]
---*/

verifyWritable(Object, "hasOwn");
verifyNotEnumerable(Object, "hasOwn");
verifyConfigurable(Object, "hasOwn");
