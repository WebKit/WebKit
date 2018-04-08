// Copyright (C) 2017 V8. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
author: Sathya Gunasekaran
description: finally calls the SpeciesConstructor
esid: sec-promise.prototype.finally
features: [Promise.prototype.finally]
---*/


class MyPromise extends Promise {
  static get[Symbol.species]() {
    return Promise;
  }
}

var p = Promise
  .resolve()
  .finally(() => MyPromise.resolve());

assert.sameValue(p instanceof Promise, true);
assert.sameValue(p instanceof MyPromise, false);
