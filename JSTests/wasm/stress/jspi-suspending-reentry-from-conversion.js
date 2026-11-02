//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// A live promising() activation somewhere on the stack is not on its own a license to suspend.
// Suspending is only legal while the promising activation's own execution context is the one
// running, which rules out any intervening JS frame. The intervening frame here is reached
// from the WasmToJS thunk's i32 result-conversion slow path: the import returns an object, so
// coercing it to i32 runs ToPrimitive, which re-enters JS. None of these coercion hooks can
// invoke the Suspending wrapper, because the wrapper is not callable, so the wrapped function
// must never run and no stack slice may be evacuated across the live C++ conversion frames.

const wat = `
(module
  (import "env" "g" (func $g (result i32)))
  (func (export "f") (result i32)
    call $g
  )
)`;

let suspendingEntered = false;

function wrapped() {
    suspendingEntered = true;
    return Promise.resolve(0x1337);
}

const wrapper = new WebAssembly.Suspending(wrapped);

// What the ordinary import hands back, chosen per case to force a particular coercion hook.
let importResult;

function g() { return importResult; }

const instance = await instantiate(wat, { env: { g } });
const f = WebAssembly.promising(instance.exports.f);

async function run(name, value) {
    suspendingEntered = false;
    importResult = value;
    let resolved, rejected;
    try {
        resolved = await f();
    } catch (e) {
        rejected = e;
    }
    assert.falsy(suspendingEntered, `${name}: wrapped suspending function must not be entered`);
    return { resolved, rejected };
}

// valueOf is not callable, so ToPrimitive skips it and falls through to
// Object.prototype.toString; "[object Object]" then coerces to 0.
{
    const { resolved, rejected } = await run("valueOf", { valueOf: wrapper });
    assert.eq(rejected, undefined, "valueOf: unexpected rejection");
    assert.eq(resolved, 0);
}

// With valueOf shadowed by a non-callable and toString not callable either, ToPrimitive has no
// usable hook left.
{
    const { rejected } = await run("toString", { valueOf: null, toString: wrapper });
    assert.truthy(rejected instanceof TypeError, "toString: expected a TypeError");
}

// GetMethod rejects a non-callable Symbol.toPrimitive outright.
{
    const { rejected } = await run("Symbol.toPrimitive", { [Symbol.toPrimitive]: wrapper });
    assert.truthy(rejected instanceof TypeError, "Symbol.toPrimitive: expected a TypeError");
}

// Same for a proxy reached during the coercion, whose get trap is not callable.
{
    const { rejected } = await run("proxy get trap", new Proxy({}, { get: wrapper }));
    assert.truthy(rejected instanceof TypeError, "proxy get trap: expected a TypeError");
}
