import Builder from '../Builder.js';
import * as assert from '../assert.js';

const pageSize = 64 * 1024;

const verbose = false;

// https://github.com/WebAssembly/design/blob/master/Modules.md#imports
// Says:
//
// A linear memory import includes the same set of fields defined in the Linear
// Memory section: initial length and optional maximum length. The host
// environment must only allow imports of WebAssembly linear memories that have
// initial length greater-or-equal than the initial length declared in the
// import and that have maximum length less-or-equal than the maximum length
// declared in the import. This ensures that separate compilation can assume:
// memory accesses below the declared initial length are always in-bounds,
// accesses above the declared maximum length are always out-of-bounds and if
// initial equals maximum, the length is fixed.

const instantiate = (builder, importObject = undefined) => {
    return new WebAssembly.Instance(
        new WebAssembly.Module(
            builder.WebAssembly().get()),
        importObject);
};

const test = (memoryToImport, importedMemoryDeclaration, growMemoryToImportBy = undefined, growImportedMemoryDeclarationBy = undefined, expectedFinal) => {
    const builder0 = (new Builder())
          .Type().End()
          .Function().End()
          .Memory().InitialMaxPages(memoryToImport.initial, memoryToImport.maximum).End()
          .Export()
              .Function("current").Function("grow").Function("get")
              .Memory("memory", 0)
          .End()
          .Code()
              .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
              .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
              .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
          .End();

    const builder1 = (new Builder())
          .Type().End()
          .Import().Memory("imp", "memory", importedMemoryDeclaration).End()
          .Function().End()
          .Export()
              .Function("current").Function("grow").Function("get")
              .Memory("memory", 0)
          .End()
          .Code()
              .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
              .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
              .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
          .End();

    const i0 = instantiate(builder0);
    const i1 = instantiate(builder1, { imp: { memory: i0.exports.memory } });
    assert.eq(i0.exports.current(), i1.exports.current());
    assert.eq(i0.exports.current(), memoryToImport.initial);

    if (growMemoryToImportBy !== undefined) {
        const grow = i0.exports.grow(growMemoryToImportBy);
        if (verbose)
            print(`currents: ${i0.exports.current()} and ${i1.exports.current()} -- grow result: ${grow}`);
    }

    if (growImportedMemoryDeclarationBy !== undefined) {
        const grow = i1.exports.grow(growImportedMemoryDeclarationBy);
        if (verbose)
            print(`currents: ${i0.exports.current()} and ${i1.exports.current()} -- grow result: ${grow}`);
    }

    assert.eq(i0.exports.current(), i1.exports.current());
    assert.eq(i0.exports.current(), expectedFinal);
};

const u = undefined;

// Identical Just Works.
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, 0, u, 2);
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, 1, u, 3);
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, 2, u, 4);
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, 3, u, 2);
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, u, 0, 2);
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, u, 1, 3);
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, u, 2, 4);
test({ initial: 2, maximum: 4 }, { initial: 2, maximum: 4 }, u, 3, 2);

// Allowed: imported initial is greater than declared.
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, 0, u, 2);
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, 1, u, 3);
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, 2, u, 4);
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, 3, u, 2);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, 0, u, 2);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, 1, u, 3);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, 2, u, 4);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, 3, u, 2);
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, u, 0, 2);
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, u, 1, 3);
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, u, 2, 4);
test({ initial: 2, maximum: 4 }, { initial: 1, maximum: 4 }, u, 3, 2);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, u, 0, 2);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, u, 1, 3);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, u, 2, 4);
test({ initial: 2, maximum: 4 }, { initial: 0, maximum: 4 }, u, 3, 2);

// Allowed: imported maximum is lesser than declared.
test({ initial: 2, maximum: 3 }, { initial: 2, maximum: 4 }, 0, u, 2);
test({ initial: 2, maximum: 3 }, { initial: 2, maximum: 4 }, 1, u, 3);
test({ initial: 2, maximum: 3 }, { initial: 2, maximum: 4 }, 2, u, 2);
test({ initial: 2, maximum: 2 }, { initial: 2, maximum: 4 }, 0, u, 2);
test({ initial: 2, maximum: 2 }, { initial: 2, maximum: 4 }, 1, u, 2);
test({ initial: 2, maximum: 3 }, { initial: 2, maximum: 4 }, u, 0, 2);
test({ initial: 2, maximum: 3 }, { initial: 2, maximum: 4 }, u, 1, 3);
test({ initial: 2, maximum: 3 }, { initial: 2, maximum: 4 }, u, 2, 2);
test({ initial: 2, maximum: 2 }, { initial: 2, maximum: 4 }, u, 0, 2);
test({ initial: 2, maximum: 2 }, { initial: 2, maximum: 4 }, u, 1, 2);

// Allowed: no declared maximum, same as above.
test({ initial: 2, maximum: 4 }, { initial: 2 }, 0, u, 2);

// Disallowed: imported initial is lesser than declared.
assert.throws(() => test({ initial: 1, maximum: 4 }, { initial: 2, maximum: 4 }, u, u, 2), WebAssembly.LinkError, `Memory import imp:memory provided a 'size' that is smaller than the module's declared 'initial' import memory size`);
assert.throws(() => test({ initial: 0, maximum: 4 }, { initial: 2, maximum: 4 }, u, u, 2), WebAssembly.LinkError, `Memory import imp:memory provided a 'size' that is smaller than the module's declared 'initial' import memory size`);

// Disallowed: imported maximum is greater than declared.
assert.throws(() => test({ initial: 2, maximum: 5 }, { initial: 2, maximum: 4 }, u, u, 2), WebAssembly.LinkError, `Memory import imp:memory provided a 'maximum' that is larger than the module's declared 'maximum' import memory size`);

// Disallowed: no imported maximum, same as above.
assert.throws(() => test({ initial: 2 }, { initial: 2, maximum: 4 }, 0, u, 2), WebAssembly.LinkError, `Memory import imp:memory did not have a 'maximum' but the module requires that it does`);
