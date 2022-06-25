// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-proxy-object-internal-methods-and-internal-slots-defineownproperty-p-desc
description: >
    Throw a TypeError exception if Desc is not configurable and target property
    descriptor is configurable and trap result is true (honoring the realm of
    the current execution context).
info: |
    [[DefineOwnProperty]] (P, Desc)

    ...
    20. Else targetDesc is not undefined,
        b. If settingConfigFalse is true and targetDesc.[[Configurable]] is
        true, throw a TypeError exception.
    ...
features: [cross-realm, Proxy]
---*/

var OProxy = $262.createRealm().global.Proxy;
var target = Object.create(null);
var p = new OProxy(target, {
  defineProperty: function() {
    return true;
  }
});

Object.defineProperty(target, 'prop', {
  value: 1,
  configurable: true
});

assert.throws(TypeError, function() {
  Object.defineProperty(p, 'prop', {
    value: 1,
    configurable: false
  });
});
