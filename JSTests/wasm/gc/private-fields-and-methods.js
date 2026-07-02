import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

function testPrivateMethodOnStruct() {
  let m = instantiate(`
    (module
      (type (struct (field i32)))
      (func (export "make") (result anyref)
        (struct.new 0 (i32.const 42)))
      (func (export "use") (param (ref 0)) (result i32)
        (struct.get 0 0 (local.get 0)))
    )
  `);
  const s = m.exports.make();

  class B { constructor() { return s; } }
  class D extends B {
    #m() {}
    constructor() { super(); }
  }

  assert.throws(
    () => new D(),
    TypeError,
    "Cannot add private method to a WebAssembly GC object"
  );

  // Struct must remain usable after the rejected attempt.
  assert.eq(m.exports.use(s), 42);
}

function testPrivateFieldOnStruct() {
  let m = instantiate(`
    (module
      (type (struct (field i32)))
      (func (export "make") (result anyref)
        (struct.new 0 (i32.const 42)))
      (func (export "use") (param (ref 0)) (result i32)
        (struct.get 0 0 (local.get 0)))
    )
  `);
  const s = m.exports.make();

  class B { constructor() { return s; } }
  class D extends B {
    #x = 1;
    constructor() { super(); }
  }

  assert.throws(
    () => new D(),
    TypeError,
    "Cannot define private field on a WebAssembly GC object"
  );

  assert.eq(m.exports.use(s), 42);
}

function testPrivateMethodOnArray() {
  let m = instantiate(`
    (module
      (type (array i32))
      (func (export "make") (result anyref)
        (array.new 0 (i32.const 7) (i32.const 3)))
      (func (export "use") (param (ref 0) i32) (result i32)
        (array.get 0 (local.get 0) (local.get 1)))
    )
  `);
  const a = m.exports.make();

  class B { constructor() { return a; } }
  class D extends B {
    #m() {}
    constructor() { super(); }
  }

  assert.throws(
    () => new D(),
    TypeError,
    "Cannot add private method to a WebAssembly GC object"
  );

  assert.eq(m.exports.use(a, 0), 7);
}

function testPrivateFieldOnArray() {
  let m = instantiate(`
    (module
      (type (array i32))
      (func (export "make") (result anyref)
        (array.new 0 (i32.const 7) (i32.const 3)))
      (func (export "use") (param (ref 0) i32) (result i32)
        (array.get 0 (local.get 0) (local.get 1)))
    )
  `);
  const a = m.exports.make();

  class B { constructor() { return a; } }
  class D extends B {
    #x = 1;
    constructor() { super(); }
  }

  assert.throws(
    () => new D(),
    TypeError,
    "Cannot define private field on a WebAssembly GC object"
  );

  assert.eq(m.exports.use(a, 0), 7);
}

function testPrivateGetterOnStruct() {
  let m = instantiate(`
    (module
      (type (struct (field i32)))
      (func (export "make") (result anyref)
        (struct.new 0 (i32.const 42)))
      (func (export "use") (param (ref 0)) (result i32)
        (struct.get 0 0 (local.get 0)))
    )
  `);
  const s = m.exports.make();

  class B { constructor() { return s; } }
  class D extends B {
    get #x() { return 1; }
    constructor() { super(); }
  }

  assert.throws(
    () => new D(),
    TypeError,
    "Cannot add private method to a WebAssembly GC object"
  );

  assert.eq(m.exports.use(s), 42);
}

function testGCSurvival() {
  let m = instantiate(`
    (module
      (type (struct (field i32)))
      (func (export "make") (result anyref)
        (struct.new 0 (i32.const 42)))
      (func (export "use") (param (ref 0)) (result i32)
        (struct.get 0 0 (local.get 0)))
    )
  `);
  const s = m.exports.make();

  class B { constructor() { return s; } }
  class D extends B {
    #m() {}
    constructor() { super(); }
  }

  try { new D(); } catch {}

  // The struct must survive GC with its structure intact.
  gc();
  assert.eq(m.exports.use(s), 42);
}

testPrivateMethodOnStruct();
testPrivateFieldOnStruct();
testPrivateMethodOnArray();
testPrivateFieldOnArray();
testPrivateGetterOnStruct();
testGCSurvival();
