// Copyright (C) 2021 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm-constructor
description: >
  The Realm constructor is extensible
info: |
  17 ECMAScript Standard Built-in Objects

  Unless specified otherwise, the [[Extensible]] internal slot of a built-in
  object initially has the value true.
features: [callable-boundary-realms]
---*/

assert.sameValue(Object.isExtensible(Realm), true);
