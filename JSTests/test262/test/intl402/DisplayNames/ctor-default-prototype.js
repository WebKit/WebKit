// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DisplayNames
description: >
  Prototype of the returned object is DisplayNames.prototype
info: |
  Intl.DisplayNames ([ locales [ , options ]])

  1. If NewTarget is undefined, throw a TypeError exception.
  2. Let displayNames be ? OrdinaryCreateFromConstructor(NewTarget, "%DisplayNamesPrototype%",
    « [[InitializedDisplayNames]], [[Locale]], [[Style]], [[Type]], [[Fallback]], [[Fields]] »).
  ...
  27. Return displayNames.
features: [Intl.DisplayNames]
---*/

var obj = new Intl.DisplayNames();

assert.sameValue(Object.getPrototypeOf(obj), Intl.DisplayNames.prototype);
