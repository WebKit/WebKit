// This file was procedurally generated from the following sources:
// - src/import-defer/super-property-set-exported.case
// - src/import-defer/ignore/ignore.template
/*---
description: _ [[Set]] exported called as super access (does not trigger execution)
esid: sec-module-namespace-exotic-objects
features: [import-defer]
flags: [generated, module]
info: |
    [[Set]] ( P, V, Receiver )
      1. return false.

---*/


import "./setup_FIXTURE.js";

import defer * as ns from "./dep_FIXTURE.js";

assert.sameValue(globalThis.evaluations.length, 0, "import defer does not trigger evaluation");

class A { constructor() { return ns; } };
class B extends A {
  constructor() {
    super();
    super.exported = 14;
  }
};

try {
  new B();
} catch (_) {}

assert.sameValue(globalThis.evaluations.length, 0, "It does not trigger evaluation");
