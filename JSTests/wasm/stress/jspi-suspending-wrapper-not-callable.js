//@ requireOptions("--useJSPI=1")
//@ skip if $secAccel3Removed

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// The Suspending constructor produces a plain wrapper object holding the wrapped function in
// an internal slot. It is deliberately not callable: binding the wrapper as a module import
// is the only thing that creates a suspending function, so the only way to reach the wrapped
// function is through a wasm import call site. A callable wrapper would let JS drive the
// stack-evacuation machinery from arbitrary call sites, where the frames beneath it are not
// the wasm frames that machinery assumes.

async function wrapped() { return 42; }

const wrapper = new WebAssembly.Suspending(wrapped);

assert.eq(typeof wrapper, "object");
assert.falsy(wrapper instanceof Function);
assert.eq(Object.getPrototypeOf(wrapper), WebAssembly.Suspending.prototype);

// The wrapped function lives in an internal slot, so it is not reachable by reflection.
assert.eq(Object.getOwnPropertyNames(wrapper).length, 0);
assert.eq(Object.getOwnPropertySymbols(wrapper).length, 0);

// Every call form has to be rejected, not just direct invocation.
assert.throws(() => wrapper(), TypeError, "");
assert.throws(() => new wrapper(), TypeError, "");
assert.throws(() => Reflect.apply(wrapper, undefined, []), TypeError, "");
assert.throws(() => Reflect.construct(wrapper, []), TypeError, "");

// The wrapper must still work where the spec does allow it: as an import.
const wat = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (func (export "entry") (result i32)
    call $get_number
  )
)`;

const instance = await instantiate(wat, { env: { get_number: wrapper } });
assert.eq(await WebAssembly.promising(instance.exports.entry)(), 42);
