// Copyright 2015 Mike Pennisi. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-getidentifierreference
es6id: 8.1.2.1
description: >
  Behavior when accessing property of `Symbol.unscopables` property throws an
  error
info: |
  [...]
  2. Let envRec be lex's EnvironmentRecord.
  3. Let exists be ? envRec.HasBinding(name).

  8.1.1.2.1 HasBinding

  [...]
  5. If the withEnvironment flag of envRec is false, return true.
  6. Let unscopables be ? Get(bindings, @@unscopables).
  7. If Type(unscopables) is Object, then
     a. Let blocked be ToBoolean(? Get(unscopables, N)).
     b. If blocked is true, return false.

  13.11.7 (The `with` Statement) Runtime Semantics: Evaluation

  [...]
  5. Set the withEnvironment flag of newEnv’s EnvironmentRecord to true.
  [...]
flags: [noStrict]
features: [Symbol.unscopables]
---*/

var env = { x: 86 };
env[Symbol.unscopables] = {};

Object.defineProperty(env[Symbol.unscopables], 'x', {
  get: function() {
    throw new Test262Error();
  }
});

with (env) {
  assert.throws(Test262Error, function() {
    x;
  });
}
