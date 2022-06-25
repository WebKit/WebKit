// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-generator.prototype.return
description: >
    A TypeError should be thrown from GeneratorValidate (25.3.3.2) if the
    context of `return` does not define the [[GeneratorState]] internal slot.
info: |
  [...]
  3. Return ? GeneratorResumeAbrupt(g, C).

  25.3.3.4 GeneratorResumeAbrupt

  1. Let state be ? GeneratorValidate(generator).

  25.3.3.2 GeneratorValidate

  [...]
  2. If generator does not have a [[GeneratorState]] internal slot, throw a
     TypeError exception.
features: [generators]
---*/

function* g() {}
var GeneratorPrototype = Object.getPrototypeOf(g).prototype;

assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call({});
  },
  'ordinary object (without value)'
);
assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call({}, 1);
  },
  'ordinary object (with value)'
);

assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call(function() {});
  },
  'function object (without value)'
);
assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call(function() {}, 1);
  },
  'function object (with value)'
);

assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call(g);
  },
  'generator function object (without value)'
);
assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call(g, 1);
  },
  'generator function object (with value)'
);

assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call(g.prototype);
  },
  'generator function prototype object (without value)'
);
assert.throws(
  TypeError,
  function() {
    GeneratorPrototype.return.call(g.prototype, 1);
  },
  'generator function prototype object (with value)'
);
