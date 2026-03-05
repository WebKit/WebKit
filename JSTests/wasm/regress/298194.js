load("../spec-harness/wasm-module-builder-gc.js");

function createWasmModule() {
    let builder = new WasmModuleBuilder();

    // Use a Subtype for B by setting isFinal = false.
    const isFinal = false;
    const supertypeIndex = kNoSuperType;

    const typeBIdx = builder.addStruct([], supertypeIndex, isFinal);
    const typeAIdx = builder.addStruct([makeField(wasmRefType(typeBIdx), true)]);

    const refA = wasmRefType(typeAIdx);
    const refB = wasmRefType(typeBIdx);

    const sigNewA = builder.addType(makeSig([refB], [refA]));
    builder.addFunction("new_A", sigNewA)
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(typeAIdx)
        ]).exportFunc();
    const sigNewB = builder.addType(makeSig([], [refB]));
    builder.addFunction("new_B", sigNewB)
        .addBody([ kGCPrefix, kExprStructNewDefault, ...wasmUnsignedLeb(typeBIdx) ]).exportFunc();

    return builder.instantiate();
}

let obj = null;
{
    let inst = createWasmModule();
    let exports = inst.exports;
    let objB = exports.new_B();
    obj = exports.new_A(objB);
}

gc();

let dummy = (new WasmModuleBuilder()).instantiate();
dummy = null;

// Trigger TypeInformation::tryCleanup() to free the TypeDefinition.
gc();

// Trigger assertion failure during GC.
gc();