// Tests that array.new and array.new_default produce correctly initialized
// arrays under OMG compilation. Exercises the fill loop generated in
// B3LowerMacros for WasmArrayNew with initValue.
//
// Each test function is called 100 000 times (OMG threshold: 50 000).

const N = 100000;

function makeInstance(bytes) {
    return new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array(bytes).buffer));
}

// ── Helper: LEB128 encoders ─────────────────────────────────────────────────

function unsignedLEB128(n) {
    const result = [];
    do {
        let byte = n & 0x7f;
        n >>>= 7;
        if (n !== 0) byte |= 0x80;
        result.push(byte);
    } while (n !== 0);
    return result;
}

function signedLEB128(n) {
    const result = [];
    let more = true;
    while (more) {
        let byte = n & 0x7f;
        n >>= 7;
        if ((n === 0 && (byte & 0x40) === 0) || (n === -1 && (byte & 0x40) !== 0))
            more = false;
        else
            byte |= 0x80;
        result.push(byte);
    }
    return result;
}

function encodeString(s) {
    const bytes = [];
    for (let i = 0; i < s.length; i++)
        bytes.push(s.charCodeAt(i));
    return [...unsignedLEB128(bytes.length), ...bytes];
}

function makeSection(id, content) {
    return [id, ...unsignedLEB128(content.length), ...content];
}

// ── i32 array.new test module ───────────────────────────────────────────────
// (module
//   (type $arr (array (mut i32)))
//   ;; array.new: create array of `size` elements all set to `val`, return arr[idx]
//   (func (export "newGet") (param $val i32) (param $size i32) (param $idx i32) (result i32)
//     (array.get 0 (array.new 0 (local.get $val) (local.get $size)) (local.get $idx)))
//   ;; array.new_default: create array, return arr[idx] (should be 0)
//   (func (export "defGet") (param $size i32) (param $idx i32) (result i32)
//     (array.get 0 (array.new_default 0 (local.get $size)) (local.get $idx)))
// )
function buildI32Module() {
    // Type section: 1 array type + 2 func types
    const typeSection = makeSection(0x01, [
        3, // 3 types
        // type 0: (array (mut i32))
        0x5e, 0x7f, 0x01,
        // type 1: (func (param i32 i32 i32) (result i32))
        0x60, 3, 0x7f, 0x7f, 0x7f, 1, 0x7f,
        // type 2: (func (param i32 i32) (result i32))
        0x60, 2, 0x7f, 0x7f, 1, 0x7f,
    ]);
    const funcSection = makeSection(0x03, [2, 1, 2]); // 2 funcs, type indices 1, 2
    const exportSection = makeSection(0x07, [
        2, // 2 exports
        ...encodeString("newGet"), 0x00, 0x00,
        ...encodeString("defGet"), 0x00, 0x01,
    ]);
    // Code section
    const func0Body = [
        0, // no locals
        0x20, 0x00, // local.get $val
        0x20, 0x01, // local.get $size
        0xfb, 0x06, 0x00, // array.new 0
        0x20, 0x02, // local.get $idx
        0xfb, 0x0b, 0x00, // array.get 0
        0x0b,
    ];
    const func1Body = [
        0, // no locals
        0x20, 0x00, // local.get $size
        0xfb, 0x07, 0x00, // array.new_default 0
        0x20, 0x01, // local.get $idx
        0xfb, 0x0b, 0x00, // array.get 0
        0x0b,
    ];
    const codeSection = makeSection(0x0a, [
        2, // 2 functions
        ...unsignedLEB128(func0Body.length), ...func0Body,
        ...unsignedLEB128(func1Body.length), ...func1Body,
    ]);
    return [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection];
}

// ── i64 array.new test module ───────────────────────────────────────────────
function buildI64Module() {
    const typeSection = makeSection(0x01, [
        3,
        // type 0: (array (mut i64))
        0x5e, 0x7e, 0x01,
        // type 1: (func (param i64 i32 i32) (result i64))
        0x60, 3, 0x7e, 0x7f, 0x7f, 1, 0x7e,
        // type 2: (func (param i32 i32) (result i64))
        0x60, 2, 0x7f, 0x7f, 1, 0x7e,
    ]);
    const funcSection = makeSection(0x03, [2, 1, 2]);
    const exportSection = makeSection(0x07, [
        2,
        ...encodeString("newGet"), 0x00, 0x00,
        ...encodeString("defGet"), 0x00, 0x01,
    ]);
    const func0Body = [
        0,
        0x20, 0x00, // local.get $val (i64)
        0x20, 0x01, // local.get $size (i32)
        0xfb, 0x06, 0x00, // array.new 0
        0x20, 0x02, // local.get $idx (i32)
        0xfb, 0x0b, 0x00, // array.get 0
        0x0b,
    ];
    const func1Body = [
        0,
        0x20, 0x00, // local.get $size
        0xfb, 0x07, 0x00, // array.new_default 0
        0x20, 0x01, // local.get $idx
        0xfb, 0x0b, 0x00, // array.get 0
        0x0b,
    ];
    const codeSection = makeSection(0x0a, [
        2,
        ...unsignedLEB128(func0Body.length), ...func0Body,
        ...unsignedLEB128(func1Body.length), ...func1Body,
    ]);
    return [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection];
}

// ── f32 array.new test module ───────────────────────────────────────────────
function buildF32Module() {
    const typeSection = makeSection(0x01, [
        3,
        // type 0: (array (mut f32))
        0x5e, 0x7d, 0x01,
        // type 1: (func (param f32 i32 i32) (result f32))
        0x60, 3, 0x7d, 0x7f, 0x7f, 1, 0x7d,
        // type 2: (func (param i32 i32) (result f32))
        0x60, 2, 0x7f, 0x7f, 1, 0x7d,
    ]);
    const funcSection = makeSection(0x03, [2, 1, 2]);
    const exportSection = makeSection(0x07, [
        2,
        ...encodeString("newGet"), 0x00, 0x00,
        ...encodeString("defGet"), 0x00, 0x01,
    ]);
    const func0Body = [
        0,
        0x20, 0x00, 0x20, 0x01, 0xfb, 0x06, 0x00,
        0x20, 0x02, 0xfb, 0x0b, 0x00, 0x0b,
    ];
    const func1Body = [
        0,
        0x20, 0x00, 0xfb, 0x07, 0x00,
        0x20, 0x01, 0xfb, 0x0b, 0x00, 0x0b,
    ];
    const codeSection = makeSection(0x0a, [
        2,
        ...unsignedLEB128(func0Body.length), ...func0Body,
        ...unsignedLEB128(func1Body.length), ...func1Body,
    ]);
    return [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection];
}

// ── f64 array.new test module ───────────────────────────────────────────────
function buildF64Module() {
    const typeSection = makeSection(0x01, [
        3,
        // type 0: (array (mut f64))
        0x5e, 0x7c, 0x01,
        // type 1: (func (param f64 i32 i32) (result f64))
        0x60, 3, 0x7c, 0x7f, 0x7f, 1, 0x7c,
        // type 2: (func (param i32 i32) (result f64))
        0x60, 2, 0x7f, 0x7f, 1, 0x7c,
    ]);
    const funcSection = makeSection(0x03, [2, 1, 2]);
    const exportSection = makeSection(0x07, [
        2,
        ...encodeString("newGet"), 0x00, 0x00,
        ...encodeString("defGet"), 0x00, 0x01,
    ]);
    const func0Body = [
        0,
        0x20, 0x00, 0x20, 0x01, 0xfb, 0x06, 0x00,
        0x20, 0x02, 0xfb, 0x0b, 0x00, 0x0b,
    ];
    const func1Body = [
        0,
        0x20, 0x00, 0xfb, 0x07, 0x00,
        0x20, 0x01, 0xfb, 0x0b, 0x00, 0x0b,
    ];
    const codeSection = makeSection(0x0a, [
        2,
        ...unsignedLEB128(func0Body.length), ...func0Body,
        ...unsignedLEB128(func1Body.length), ...func1Body,
    ]);
    return [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection];
}

// ── Test 1: i32 array.new ───────────────────────────────────────────────────
{
    const { newGet, defGet } = makeInstance(buildI32Module()).exports;
    for (let i = 0; i < N; i++) {
        const r = newGet(42, 4, 0);
        if (r !== 42)
            throw new Error(`i32 newGet(42,4,0): expected 42, got ${r}`);
    }
    // Check various indices
    for (let idx = 0; idx < 4; idx++) {
        const r = newGet(99, 4, idx);
        if (r !== 99)
            throw new Error(`i32 newGet(99,4,${idx}): expected 99, got ${r}`);
    }
    // Larger size
    for (let i = 0; i < N; i++) {
        const r = newGet(7, 10000, 9999);
        if (r !== 7)
            throw new Error(`i32 newGet(7,10000,9999): expected 7, got ${r}`);
    }
    // array.new_default
    for (let i = 0; i < N; i++) {
        const r = defGet(100, 50);
        if (r !== 0)
            throw new Error(`i32 defGet(100,50): expected 0, got ${r}`);
    }
}

// ── Test 2: i64 array.new ───────────────────────────────────────────────────
{
    const { newGet, defGet } = makeInstance(buildI64Module()).exports;
    for (let i = 0; i < N; i++) {
        const r = newGet(123n, 4, 0);
        if (r !== 123n)
            throw new Error(`i64 newGet(123n,4,0): expected 123n, got ${r}`);
    }
    for (let i = 0; i < N; i++) {
        const r = newGet(0x1_0000_0000n, 100, 99);
        if (r !== 0x1_0000_0000n)
            throw new Error(`i64 newGet large: expected 0x100000000n, got ${r}`);
    }
    for (let i = 0; i < N; i++) {
        const r = defGet(100, 50);
        if (r !== 0n)
            throw new Error(`i64 defGet(100,50): expected 0n, got ${r}`);
    }
}

// ── Test 3: f32 array.new ───────────────────────────────────────────────────
{
    const { newGet, defGet } = makeInstance(buildF32Module()).exports;
    for (let i = 0; i < N; i++) {
        const r = newGet(3.14, 4, 0);
        if (Math.abs(r - 3.14) > 0.001)
            throw new Error(`f32 newGet(3.14,4,0): expected ~3.14, got ${r}`);
    }
    for (let i = 0; i < N; i++) {
        const r = defGet(100, 50);
        if (r !== 0)
            throw new Error(`f32 defGet(100,50): expected 0, got ${r}`);
    }
}

// ── Test 4: f64 array.new ───────────────────────────────────────────────────
{
    const { newGet, defGet } = makeInstance(buildF64Module()).exports;
    for (let i = 0; i < N; i++) {
        const r = newGet(2.718281828, 4, 0);
        if (Math.abs(r - 2.718281828) > 1e-9)
            throw new Error(`f64 newGet(2.718281828,4,0): expected ~2.718, got ${r}`);
    }
    for (let i = 0; i < N; i++) {
        const r = newGet(1.5, 10000, 9999);
        if (r !== 1.5)
            throw new Error(`f64 newGet(1.5,10000,9999): expected 1.5, got ${r}`);
    }
    for (let i = 0; i < N; i++) {
        const r = defGet(100, 50);
        if (r !== 0)
            throw new Error(`f64 defGet(100,50): expected 0, got ${r}`);
    }
}
