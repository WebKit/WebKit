import * as assert from '../assert.js';

// Mixing the legacy exception opcodes with try_table is invalid, but neither is visible until
// the bodies using them are parsed. With bodies parsed on first call, the rule can only be
// applied to the body that introduces the second of the two styles -- and only to that body: a
// function that uses neither style is unaffected by what the rest of the module does.

// (module
//   (import "env" "throw" (func $jsThrow))
//   (func $callRethrow (try $try (do (call $jsThrow)) (catch_all (rethrow $try))))
//   (func $throwRef (param $exnref exnref) (throw_ref (local.get $exnref)))
//   (func (export "trigger")
//     (call $throwRef (block $block (result exnref)
//       (try_table (catch_all_ref $block) (call $callRethrow) (return)))))
// )
const bytes = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x00, 0x60, 0x01, 0x69, 0x00, 0x02, 0x0d, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x03, 0x04, 0x03, 0x00, 0x01, 0x00, 0x07, 0x0b, 0x01, 0x07, 0x74, 0x72, 0x69, 0x67, 0x67, 0x65, 0x72, 0x00, 0x03, 0x0a, 0x24, 0x03, 0x0a, 0x00, 0x06, 0x40, 0x10, 0x00, 0x19, 0x09, 0x00, 0x0b, 0x0b, 0x05, 0x00, 0x20, 0x00, 0x0a, 0x0b, 0x11, 0x00, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x10, 0x01, 0x0f, 0x0b, 0x00, 0x0b, 0x10, 0x02, 0x0b]);

assert.falsy(WebAssembly.validate(bytes));

const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {
    env: { throw() { } }
});

for (let i = 0; i < 2; ++i) {
    let caught;
    try {
        instance.exports.trigger();
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
    assert.truthy(caught.message.includes("Module uses both legacy exceptions and try_table"),
        `unexpected message: ${caught.message}`);
}

assert.compileError(bytes, "Module uses both legacy exceptions and try_table");

// A module holding both styles in separate functions, plus one function that uses neither. Which
// of the first two is rejected depends on which is reached first, but the third is always
// callable, and rejecting one function never makes the others stop working.
function leb(value) {
    let result = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        result.push(byte);
    } while (value);
    return result;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

function functionBody(code) {
    const withLocals = [0x00, ...code];
    return [...leb(withLocals.length), ...withLocals];
}

function exportEntry(name, index) {
    const utf8 = [...name].map(c => c.charCodeAt(0));
    return [...leb(utf8.length), ...utf8, 0x00, ...leb(index)];
}

const legacyBody = [
    0x06, 0x40,  // try
    0x19,        // catch_all
    0x0b,        // end try
    0x0b,        // end function
];

const modernBody = [
    0x02, 0x40,              // block
    0x1f, 0x40, 0x01,        // try_table, 1 catch clause
    0x02, 0x00,              //   catch_all -> the enclosing block
    0x0b,                    // end try_table
    0x0b,                    // end block
    0x0b,                    // end function
];

const plainBody = [
    0x41, 0x07,  // i32.const 7
    0x0b,        // end function
];

const mixedBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ...section(1, [0x02, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7f]),
    ...section(3, [0x03, 0x00, 0x00, 0x01]),
    ...section(7, [0x03, ...exportEntry("legacy", 0), ...exportEntry("modern", 1), ...exportEntry("plain", 2)]),
    ...section(10, [0x03, ...functionBody(legacyBody), ...functionBody(modernBody), ...functionBody(plainBody)]),
]);

assert.falsy(WebAssembly.validate(mixedBytes));

function callExpectingSuccess(exports, name) {
    exports[name]();
}

function callExpectingMixedProposalError(exports, name) {
    let caught;
    try {
        exports[name]();
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError, name);
    assert.truthy(caught.message.includes("Module uses both legacy exceptions and try_table"),
        `unexpected message for ${name}: ${caught.message}`);
}

for (const [first, second] of [["legacy", "modern"], ["modern", "legacy"]]) {
    const exports = new WebAssembly.Instance(new WebAssembly.Module(mixedBytes)).exports;
    callExpectingSuccess(exports, first);
    assert.eq(exports.plain(), 7);
    callExpectingMixedProposalError(exports, second);
    // Rejecting the function that brought in the second style says nothing about a function
    // that uses neither, whether it was parsed before that happened or after.
    assert.eq(exports.plain(), 7);
    const freshExports = new WebAssembly.Instance(new WebAssembly.Module(mixedBytes)).exports;
    callExpectingSuccess(freshExports, first);
    callExpectingMixedProposalError(freshExports, second);
    assert.eq(freshExports.plain(), 7);
}

// A body turned away for bringing in the second style claims nothing for the module, so a later
// body using the style that was accepted first is still accepted.
const twoOfEachBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ...section(1, [0x01, 0x60, 0x00, 0x00]),
    ...section(3, [0x04, 0x00, 0x00, 0x00, 0x00]),
    ...section(7, [0x04, ...exportEntry("legacy", 0), ...exportEntry("modern", 1), ...exportEntry("legacy2", 2), ...exportEntry("modern2", 3)]),
    ...section(10, [0x04, ...functionBody(legacyBody), ...functionBody(modernBody), ...functionBody(legacyBody), ...functionBody(modernBody)]),
]);

for (const [first, second] of [["legacy", "modern"], ["modern", "legacy"]]) {
    const exports = new WebAssembly.Instance(new WebAssembly.Module(twoOfEachBytes)).exports;
    callExpectingSuccess(exports, first);
    callExpectingMixedProposalError(exports, second);
    callExpectingSuccess(exports, first + "2");
    callExpectingMixedProposalError(exports, second + "2");
}

// An unexecuted tiny direct-call target inspected during OMG inlining must not claim an
// exception proposal for the module before any function has actually called it.
{
    const hotBodyWithLocals = [
        0x01, 0x01, 0x7f,              // 1 local of type i32 (index 1)
        0x20, 0x00,                    // local.get 0 (cond)
        0x04, 0x40,                    // if
        0x10, 0x00,                    //   call 0 (deadLegacy)
        0x0b,                          // end if
        0x03, 0x40,                    // loop
        0x20, 0x01, 0x41, 0x01, 0x6a,  //   local.get 1; i32.const 1; i32.add
        0x22, 0x01,                    //   local.tee 1
        0x41, 0xc0, 0x00, 0x49,        //   i32.const 64; i32.lt_u
        0x0d, 0x00,                    //   br_if 0
        0x0b,                          // end loop
        0x20, 0x01,                    // local.get 1
        0x0b,                          // end function
    ];

    const deadInliningBytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...section(1, [0x02, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7f, 0x01, 0x7f]),
        ...section(3, [0x03, 0x00, 0x01, 0x00]),
        ...section(7, [0x02, ...exportEntry("hot", 1), ...exportEntry("modern", 2)]),
        ...section(10, [
            0x03,
            ...functionBody(legacyBody),
            ...leb(hotBodyWithLocals.length), ...hotBodyWithLocals,
            ...functionBody(modernBody),
        ]),
    ]);

    const exports = new WebAssembly.Instance(new WebAssembly.Module(deadInliningBytes)).exports;
    for (let i = 0; i < 600; ++i)
        assert.eq(exports.hot(0), 64);
    exports.modern();

    let caught;
    try {
        exports.hot(1);
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
    assert.truthy(caught.message.includes("Module uses both legacy exceptions and try_table, in function at index 0"),
        `unexpected message: ${caught.message}`);
}

// Concurrent eager validation of a module mixing legacy exceptions and try_table across worker
// batches must deterministically report the module-level error rather than racing on function indices.
{
    const slowLegacyCode = [...Array(6000).fill(0x01), ...legacyBody];
    const concurrentMixedBytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...section(1, [0x01, 0x60, 0x00, 0x00]),
        ...section(3, [0x02, 0x00, 0x00]),
        ...section(10, [0x02, ...functionBody(slowLegacyCode), ...functionBody(modernBody)]),
    ]);

    let baselineMessage;
    for (let i = 0; i < 20; ++i) {
        let caught;
        try {
            new WebAssembly.Module(concurrentMixedBytes, { eagerValidate: true });
        } catch (e) {
            caught = e;
        }
        assert.instanceof(caught, WebAssembly.CompileError);
        assert.truthy(caught.message.includes("Module uses both legacy exceptions and try_table"),
            `unexpected message: ${caught.message}`);
        assert.falsy(caught.message.includes("in function at index"),
            `mixed-proposal module error should not depend on worker scheduling: ${caught.message}`);
        if (baselineMessage === undefined)
            baselineMessage = caught.message;
        else
            assert.eq(caught.message, baselineMessage);
    }
}
