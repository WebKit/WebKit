//@ skip if $architecture != "arm64"
//@ requireOptions("--useExecutableAllocationFuzz=false", "--enableWasmDebugger=true")
import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";


async function trapStack(moduleName) {
    const wat = `
(module $${moduleName}
  (func (export "run")
    unreachable
  )
)`;
    const inst = await instantiate(wat, {}, { write_debug_names: true });
    try {
        inst.exports.run();
        throw new Error("expected trap");
    } catch (e) {
        assert.eq(e instanceof WebAssembly.RuntimeError, true);
        return String(e.stack);
    }
}

function assertHasModuleFrame(stack, moduleName) {
    const needle = `${moduleName}:wasm-function[0]`;
    assert.truthy(stack.includes(needle), `expected ${needle} in:\n${stack}`);
}

const stackA = await trapStack("modA");
const stackB = await trapStack("modB");
assertHasModuleFrame(stackA, "modA");
assertHasModuleFrame(stackB, "modB");
assert.eq(stackA.includes("modB:wasm-function"), false);
assert.eq(stackB.includes("modA:wasm-function"), false);

print("stack-trace-module-name: ok");
