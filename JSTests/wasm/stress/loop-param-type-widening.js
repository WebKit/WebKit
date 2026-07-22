function uleb128(n) {
    const r = [];
    do {
        let b = n & 0x7f;
        n >>>= 7;
        if (n) b |= 0x80;
        r.push(b);
    } while (n);
    return r;
}
function encodeString(s) {
    const b = [];
    for (let i = 0; i < s.length; i++) b.push(s.charCodeAt(i));
    return [...uleb128(b.length), ...b];
}
function section(id, content) { return [id, ...uleb128(content.length), ...content]; }

// --- Part 1 ---------------------------------------------------------------
// A loop declaring `(param anyref)` entered with a `(ref $0)` on the stack must
// typecheck its body against `anyref`, so `struct.get 0 0` at the top of the body
// is a validation error. Previously the body was typechecked against the narrower
// `(ref $0)` and the module was (unsoundly) accepted.
{
    // Type 0: struct { i64 mut }
    // Type 1: func (anyref) -> (i64)           — loop block signature
    // Type 2: func (i32, externref, ref 0) -> (i64)
    // Type 3: func () -> (ref 0)
    const typeSection = section(1, [
        0x04,
        0x5F, 0x01, 0x7E, 0x01,
        0x60, 0x01, 0x6E, 0x01, 0x7E,
        0x60, 0x03, 0x7F, 0x6F, 0x64, 0x00, 0x01, 0x7E,
        0x60, 0x00, 0x01, 0x64, 0x00,
    ]);
    const funcSection = section(3, [0x02, 0x02, 0x03]);
    const exportSection = section(7, [
        0x02,
        ...encodeString("f"), 0x00, 0x00,
        ...encodeString("make"), 0x00, 0x01,
    ]);
    const body0 = [
        0x01, 0x01, 0x7E,       // 1 local: i64
        0x20, 0x02,             // local.get 2 (ref $0)
        0x03, 0x01,             // loop (type 1)  — param anyref
        0xFB, 0x02, 0x00, 0x00, //   struct.get 0 0    <-- must fail: anyref !<: (ref null $0)
        0x21, 0x03,             //   local.set 3
        0x20, 0x01,             //   local.get 1 (externref)
        0xFB, 0x1A,             //   any.convert_extern
        0x20, 0x00,             //   local.get 0
        0x0D, 0x00,             //   br_if 0
        0x1A,                   //   drop
        0x20, 0x03,             //   local.get 3
        0x0B,                   // end loop
        0x0B,                   // end func
    ];
    const body1 = [0x00, 0x42, 0x00, 0xFB, 0x00, 0x00, 0x0B]; // i64.const 0; struct.new 0
    const codeSection = section(10, [
        0x02,
        ...uleb128(body0.length), ...body0,
        ...uleb128(body1.length), ...body1,
    ]);
    const bin = new Uint8Array([
        0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection,
    ]);

    if (WebAssembly.validate(bin))
        throw new Error("Part 1: module with struct.get on anyref loop param must not validate");
}

// --- Part 2 ---------------------------------------------------------------
// A loop declaring `(param (ref null $0))` entered with a non-null `(ref $0)` is
// valid, but the body must be compiled against the nullable type: struct.get must
// emit its null check so a null delivered on the back-edge traps cleanly.
{
    // Type 0: struct { i64 mut }
    // Type 1: func (ref null 0) -> (i64)       — loop block signature
    // Type 2: func (i32, ref 0) -> (i64)
    // Type 3: func () -> (ref 0)
    const typeSection = section(1, [
        0x04,
        0x5F, 0x01, 0x7E, 0x01,
        0x60, 0x01, 0x63, 0x00, 0x01, 0x7E,
        0x60, 0x02, 0x7F, 0x64, 0x00, 0x01, 0x7E,
        0x60, 0x00, 0x01, 0x64, 0x00,
    ]);
    const funcSection = section(3, [0x02, 0x02, 0x03]);
    const exportSection = section(7, [
        0x02,
        ...encodeString("f"), 0x00, 0x00,
        ...encodeString("make"), 0x00, 0x01,
    ]);
    const body0 = [
        0x01, 0x01, 0x7E,       // 1 local: i64
        0x20, 0x01,             // local.get 1 (ref $0, non-null)
        0x03, 0x01,             // loop (type 1)  — param (ref null $0)
        0xFB, 0x02, 0x00, 0x00, //   struct.get 0 0    <-- must keep null check
        0x21, 0x02,             //   local.set 2
        0xD0, 0x71,             //   ref.null none
        0x20, 0x00,             //   local.get 0
        0x0D, 0x00,             //   br_if 0        <-- back-edge with null
        0x1A,                   //   drop
        0x20, 0x02,             //   local.get 2
        0x0B,                   // end loop
        0x0B,                   // end func
    ];
    const body1 = [0x00, 0x42, 0x2A, 0xFB, 0x00, 0x00, 0x0B]; // i64.const 42; struct.new 0
    const codeSection = section(10, [
        0x02,
        ...uleb128(body0.length), ...body0,
        ...uleb128(body1.length), ...body1,
    ]);
    const bin = new Uint8Array([
        0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection,
    ]);

    if (!WebAssembly.validate(bin))
        throw new Error("Part 2: module must validate");
    const inst = new WebAssembly.Instance(new WebAssembly.Module(bin));
    const s = inst.exports.make();

    for (let i = 0; i < wasmTestLoopCount; i++) {
        if (inst.exports.f(0, s) !== 42n)
            throw new Error("Part 2: expected 42");
    }

    let trapped = false;
    try {
        inst.exports.f(1, s);
    } catch (e) {
        if (!(e instanceof WebAssembly.RuntimeError))
            throw new Error("Part 2: expected WebAssembly.RuntimeError, got " + e);
        trapped = true;
    }
    if (!trapped)
        throw new Error("Part 2: expected null-dereference trap on back-edge");
}
