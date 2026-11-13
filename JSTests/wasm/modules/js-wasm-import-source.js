//@ requireOptions("--useSourcePhaseImports=true")
import * as assert from '../assert.js';
import source wasmModule from "./constant.wasm"

assert.eq(wasmModule instanceof WebAssembly.Module, true);
const AbstractModuleSource = Object.getPrototypeOf(WebAssembly.Module);
assert.eq(AbstractModuleSource.name, "AbstractModuleSource");
assert.eq(wasmModule instanceof AbstractModuleSource, true);

const instance = new WebAssembly.Instance(wasmModule);
assert.eq(instance.exports.constant.value, 42);

assert.throws(() => {
    wasmModule = 1;
}, TypeError, "Attempted to assign to readonly property.");

const dynamicSource = await import.source("./constant.wasm");
assert.eq(dynamicSource, wasmModule);

try {
    await import.source("../assert.js");
    throw new Error("import.source of a JS module should throw");
} catch (error) {
    if (!(error instanceof SyntaxError))
        throw new Error("import.source of a JS module should be a SyntaxError");
}

try {
    await import.source("./resources/js-source-phase-has-missing-import.js");
    throw new Error("import.source of a JS module with a missing import should throw");
} catch (error) {
    if (!(error instanceof SyntaxError))
        throw new Error("expected SyntaxError for source-phase JS, got " + error);
}
