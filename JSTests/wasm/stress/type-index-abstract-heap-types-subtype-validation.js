//@ slow!
// https://bugs.webkit.org/show_bug.cgi?id=247454
import * as assert from "../assert.js";

const concreteCast = read("type-index-abstract-heap-types-subtype-validation-concrete-cast.wasm", "binary");
const funcrefToExternref = read("type-index-abstract-heap-types-subtype-validation-funcref-to-externref.wasm", "binary");
const externrefToAnyref = read("type-index-abstract-heap-types-subtype-validation-externref-to-anyref.wasm", "binary");
const i31refToAnyref = read("type-index-abstract-heap-types-subtype-validation-i31ref-to-anyref.wasm", "binary");
const structrefToEqref = read("type-index-abstract-heap-types-subtype-validation-structref-to-eqref.wasm", "binary");

function testSubtypeValidation() {
    new WebAssembly.Module(concreteCast);

    assert.throws(
        () => new WebAssembly.Module(funcrefToExternref),
        WebAssembly.CompileError,
        "ref.cast"
    );

    assert.throws(
        () => new WebAssembly.Module(externrefToAnyref),
        WebAssembly.CompileError,
        "ref.cast"
    );

    new WebAssembly.Module(i31refToAnyref);

    new WebAssembly.Module(structrefToEqref);
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testSubtypeValidation();
