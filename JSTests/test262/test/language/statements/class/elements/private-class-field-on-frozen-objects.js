// Copyright (C) 2019 Caio Lima. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: It is possible to add private fields on frozen objects
esid: sec-define-field
info: |
  DefineField(receiver, fieldRecord)
    ...
    8. If fieldName is a Private Name,
      a. Perform ? PrivateFieldAdd(fieldName, receiver, initValue).
    9. Else,
      a. Assert: IsPropertyKey(fieldName) is true.
      b. Perform ? CreateDataPropertyOrThrow(receiver, fieldName, initValue).
    10. Return.
features: [class, class-fields-private, class-fields-public]
flags: [onlyStrict]
---*/

class Test {
  f = this;
  #g = (Object.freeze(this), "Test262");

  get g() {
    return this.#g;
  }
}

let t = new Test();
assert.sameValue(t.f, t);
assert.sameValue(t.g, "Test262");
