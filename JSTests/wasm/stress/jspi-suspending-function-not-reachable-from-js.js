//@ requireOptions("--useJSPI=1")
//@ skip if $secAccel3Removed

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// A suspending import can be handed back to JS as an ordinary callable: by re-exporting it, by
// reading it out of a funcref table, or by returning it as a funcref result. Invoked that way
// there is no Wasm import frame beneath it and so nothing to evacuate, and it has to be refused
// before the wrapped function runs and before the suspension machinery touches the current VM
// entry frame. A promising() activation live elsewhere on the stack does not make the call legal,
// because what may be suspended is that activation's own execution, not whatever happens to be
// running when the call is made.

const wat = `
(module
  (import "env" "s" (func $s (result i32)))
  (export "reexported" (func $s))
  (table (export "t") 1 funcref)
  (elem (i32.const 0) $s)
  (func (export "getRef") (result funcref)
    ref.func $s
  )
)`;

let entered = false;

function wrapped() {
    entered = true;
    return Promise.resolve(1);
}

const instance = await instantiate(wat, { env: { s: new WebAssembly.Suspending(wrapped) } });

const handles = [
    ["re-export", instance.exports.reexported],
    ["table.get", instance.exports.t.get(0)],
    ["funcref result", instance.exports.getRef()],
];

for (const [name, handle] of handles) {
    assert.isFunction(handle, `${name}: expected a callable`);
    entered = false;
    assert.throws(handle, WebAssembly.SuspendError, "");
    assert.falsy(entered, `${name}: wrapped function must not be entered`);
}

// The same three handles, now reached from inside a promising() activation.
const outerWat = `
(module
  (import "env" "cb" (func $cb (result i32)))
  (func (export "run") (result i32)
    call $cb
  )
)`;

for (const [name, handle] of handles) {
    entered = false;
    let inner;
    const outer = await instantiate(outerWat, {
        env: {
            cb: () => {
                try {
                    handle();
                    inner = "no exception";
                } catch (e) {
                    inner = e;
                }
                return 0;
            }
        }
    });

    await WebAssembly.promising(outer.exports.run)();

    assert.truthy(inner instanceof WebAssembly.SuspendError,
        `${name} under a promising activation: expected a SuspendError, got ${inner}`);
    assert.falsy(entered,
        `${name} under a promising activation: wrapped function must not be entered`);
}
