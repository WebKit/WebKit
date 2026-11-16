//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// SuspendError is otherwise only exercised by the imported web-platform-tests, which run in a
// single configuration. Driving these cases from here runs them through the wasm stress modes
// (BBQ, OMG, IPInt, no-JIT, ...), whose differing frame layouts are what the suspend checks
// have to reason about.

const oneImport = `
(module
  (import "env" "s" (func $s (result i32)))
  (func (export "entry") (result i32)
    call $s
  )
)`;

// Suspending with no promising activation anywhere on the stack.
{
    const instance = await instantiate(oneImport, {
        env: { s: new WebAssembly.Suspending(async () => 42) }
    });
    assert.throws(() => instance.exports.entry(), WebAssembly.SuspendError, "");
}

// A plain JS import sits between the promising activation and the suspending import, so the
// frames that would be evacuated include JS frames.
const twoImports = `
(module
  (import "env" "plain" (func $plain (result i32)))
  (import "env" "suspending" (func $suspending (result i32)))
  (func (export "outer") (result i32)
    call $plain
  )
  (func (export "inner") (result i32)
    call $suspending
  )
)`;

{
    let instance;
    const plain = () => instance.exports.inner();

    instance = await instantiate(twoImports, {
        env: {
            plain,
            suspending: new WebAssembly.Suspending(async () => 42)
        }
    });

    let rejected;
    try {
        await WebAssembly.promising(instance.exports.outer)();
    } catch (e) {
        rejected = e;
    }
    assert.truthy(rejected instanceof WebAssembly.SuspendError,
        `expected a SuspendError, got ${rejected}`);
}
