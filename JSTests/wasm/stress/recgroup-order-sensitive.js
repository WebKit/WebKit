// Recursion group identity is ORDER-sensitive: two recgroups that contain
// the same set of type members in different order must produce DISTINCT
// canonical RTTs. The "type at index i of recgroup X" identity includes i.
//
// Without order sensitivity, a struct.new of module A's $t0 (declared as
// (struct i32)) would be accepted as module B's $t0 (declared as
// (struct f32)) just because both recgroups contain the same two types.
//
// See: Source/JavaScriptCore/wasm/WasmTypeDefinition.cpp:
// CanonicalRecursionGroupEntryHash::hash/equal --- they walk the rtts
// vector positionally (pairIntHash is non-commutative, and equal compares
// a.rtts[i] with b.rtts[i]). This test locks that invariant in.

function u(n) {
    const out = [];
    do { let b = n & 0x7f; n >>>= 7; if (n) b |= 0x80; out.push(b); } while (n);
    return out;
}

function section(id, body) {
    return [id, ...u(body.length), ...body];
}

function buildModule(structKinds) {
    // structKinds: array of ['i32' | 'f32'] describing each struct's single field
    const KIND = { i32: 0x7f, f32: 0x7d };

    // Type section: 1 top-level rec group containing structKinds.length struct types.
    // Plus an auxiliary func signature for each (to support struct.new exports).
    // Encoded with `(rec ...)` as a single top-level entry (0x4e, count, types...),
    // followed by one func type per struct (for the export functions).
    const typeBody = [];
    typeBody.push(...u(1 + structKinds.length)); // total top-level entries: 1 rec + func sigs
    typeBody.push(0x4e); typeBody.push(...u(structKinds.length));
    for (const k of structKinds) {
        typeBody.push(0x5f, 0x01, KIND[k], 0x01); // struct, 1 field, type, mut
    }
    // Func sigs: () -> (ref $ti)  and (anyref) -> i32  for ref.test
    // We use type index 0, 1, ... for the structs in the rec group.
    // structMakeSigIdx[i] = structKinds.length + i
    for (let i = 0; i < structKinds.length; ++i) {
        // () -> (ref $ti)
        typeBody.push(0x60, 0x00, 0x01, 0x64, i);  // func, 0 params, 1 result: (ref ti)
    }

    // Function section: for each struct, declare function using the matching sig.
    const funcBody = [...u(structKinds.length)];
    for (let i = 0; i < structKinds.length; ++i)
        funcBody.push(...u(structKinds.length + i)); // sig index

    // Export section: export makeT{i} per struct.
    const exportBody = [...u(structKinds.length)];
    for (let i = 0; i < structKinds.length; ++i) {
        const name = `makeT${i}`;
        exportBody.push(name.length);
        for (const c of name) exportBody.push(c.charCodeAt(0));
        exportBody.push(0x00, i); // export kind=func, func index
    }

    // Code section: for each function, struct.new $ti with constant of the field kind.
    const codeBody = [...u(structKinds.length)];
    for (let i = 0; i < structKinds.length; ++i) {
        const local = [0x00]; // 0 local decls
        const body = [];
        if (structKinds[i] === 'i32')
            body.push(0x41, 42);         // i32.const 42
        else
            body.push(0x43, 0x00, 0x00, 0x60, 0x40); // f32.const 3.5
        body.push(0xfb, 0x00, i); // struct.new $ti (GC prefix 0xfb, 0x00 = struct.new, type index)
        body.push(0x0b); // end
        const fn = [...local, ...body];
        codeBody.push(...u(fn.length));
        codeBody.push(...fn);
    }

    const bytes = [
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...section(0x01, typeBody),
        ...section(0x03, funcBody),
        ...section(0x07, exportBody),
        ...section(0x0a, codeBody),
    ];
    return new Uint8Array(bytes);
}

function buildTestModule(structKinds) {
    // A module whose $t0 is (struct structKinds[0]) and that exports
    // testT0(anyref) -> i32 using ref.test against (ref $t0).
    const KIND = { i32: 0x7f, f32: 0x7d };
    const typeBody = [];
    typeBody.push(...u(2)); // rec + test sig
    typeBody.push(0x4e); typeBody.push(...u(structKinds.length));
    for (const k of structKinds) {
        typeBody.push(0x5f, 0x01, KIND[k], 0x01);
    }
    // func (anyref) -> i32
    typeBody.push(0x60, 0x01, 0x6e, 0x01, 0x7f);

    const funcBody = [...u(1), ...u(structKinds.length)]; // func uses last sig
    const exportBody = [...u(1)];
    const ename = 'testT0';
    exportBody.push(ename.length);
    for (const c of ename) exportBody.push(c.charCodeAt(0));
    exportBody.push(0x00, 0x00);

    const code = [];
    code.push(0x00); // no locals
    code.push(0x20, 0x00); // local.get 0
    code.push(0xfb, 0x14, 0x00); // ref.test (ref $t0) -- GC prefix, ref.test opcode, type index
    code.push(0x0b);
    const codeBody = [...u(1), ...u(code.length), ...code];

    const bytes = [
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...section(0x01, typeBody),
        ...section(0x03, funcBody),
        ...section(0x07, exportBody),
        ...section(0x0a, codeBody),
    ];
    return new Uint8Array(bytes);
}

// Module A: rec (struct i32) (struct f32). Member 0 = struct of i32.
const A = new WebAssembly.Module(buildModule(['i32', 'f32']));
// Module B: rec (struct f32) (struct i32). Member 0 = struct of f32.
// $t0 structurally differs from A's $t0 (different field kind), so
// ref.test of A's $t0 instance against B's $t0 must fail.
const B = new WebAssembly.Module(buildTestModule(['f32', 'i32']));

const a = new WebAssembly.Instance(A).exports;
const b = new WebAssembly.Instance(B).exports;

const aT0 = a.makeT0(); // instance of A's $t0 = (struct i32)
const result = b.testT0(aT0);
if (result !== 0)
    throw new Error(`order-sensitivity broken: B::testT0 accepted A::$t0 (expected 0, got ${result})`);

// Sanity check: a module with the SAME order should accept it.
const B_same = new WebAssembly.Module(buildTestModule(['i32', 'f32']));
const b_same = new WebAssembly.Instance(B_same).exports;
const okResult = b_same.testT0(aT0);
if (okResult !== 1)
    throw new Error(`same-order canonical identity broken: expected 1, got ${okResult}`);
