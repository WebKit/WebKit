import * as assert from '../assert.js';

// A function body is parsed on the first call into it, so a body that fails validation is
// reported then rather than when the module is compiled. These check what that report looks
// like, and that asking again gives the same answer rather than parsing the body a second
// time.

// (module
//   (func (export "bad") (result i32))   ;; declares i32, pushes nothing
//   (func (export "good") (result i32) i32.const 42)
// )
const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,       // type: () -> i32
    0x03, 0x03, 0x02, 0x00, 0x00,                   // two functions of type 0
    0x07, 0x0e, 0x02,                               // exports
    0x03, 0x62, 0x61, 0x64, 0x00, 0x00,             //   "bad" -> func 0
    0x04, 0x67, 0x6f, 0x6f, 0x64, 0x00, 0x01,       //   "good" -> func 1
    0x0a, 0x09, 0x02,
    0x02, 0x00, 0x0b,                               //   func 0: end
    0x04, 0x00, 0x41, 0x2a, 0x0b,                   //   func 1: i32.const 42; end
]);

assert.falsy(WebAssembly.validate(bytes));

const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));

// A sibling whose body does parse is unaffected by the one that does not.
assert.eq(instance.exports.good(), 42);

let first;
for (let i = 0; i < 10; ++i) {
    let caught;
    try {
        instance.exports.bad();
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
    // The parser's own diagnostic, not a generic "function body failed to parse".
    assert.truthy(caught.message.includes("block with type"),
        `expected the parser diagnostic, got: ${caught.message}`);
    assert.truthy(caught.message.includes("in function at index 0"),
        `expected failing function index, got: ${caught.message}`);
    if (first === undefined)
        first = caught.message;
    else
        assert.eq(caught.message, first);
}

// A body of zero bytes cannot hold the locals vector and trailing End every body has, so it
// is malformed framing rather than a body to validate lazily, and is rejected at compile time
// however the module is validated.
{
    const emptyBody = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,     // type: () -> ()
        0x03, 0x02, 0x01, 0x00,                 // one function of type 0
        0x0a, 0x02, 0x01, 0x00,                 // code: one body, size 0
    ]);
    assert.falsy(WebAssembly.validate(emptyBody));
    assert.throws(() => new WebAssembly.Module(emptyBody), WebAssembly.CompileError, "");
}

// Eager validation reports the same body at compile time instead.
{
    let caught;
    try {
        new WebAssembly.Module(bytes, { eagerValidate: true });
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
    assert.truthy(caught.message.includes("block with type"));
    assert.truthy(caught.message.includes("in function at index 0"));
}

// Calling an invalid internal helper through a valid exported caller reports the helper's index.
{
    const callerBytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,       // type: () -> i32
        0x03, 0x03, 0x02, 0x00, 0x00,                   // two functions of type 0
        0x07, 0x0a, 0x01, 0x06, 0x63, 0x61, 0x6c, 0x6c, 0x65, 0x72, 0x00, 0x00, // export "caller" -> func 0
        0x0a, 0x09, 0x02,
        0x04, 0x00, 0x10, 0x01, 0x0b,                   // func 0: call 1; end
        0x02, 0x00, 0x0b,                               // func 1: end (invalid)
    ]);
    const callerInstance = new WebAssembly.Instance(new WebAssembly.Module(callerBytes));
    let caught;
    try {
        callerInstance.exports.caller();
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
    assert.truthy(caught.message.includes("in function at index 1"),
        `expected failing helper index 1, got: ${caught.message}`);
}

// A module's start function runs unconditionally during instantiation, so an invalid start body
// must be rejected before active element or data segments mutate imported tables or memories.
{
    function makeStartModule(startBody) {
        return new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x04, 0x01, 0x60, 0x00, 0x00,                                     // type 0: () -> ()
            0x02, 0x18, 0x02,                                                       // 2 imports:
            0x03, 0x65, 0x6e, 0x76, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x01,       //   env.mem (memory 1)
            0x03, 0x65, 0x6e, 0x76, 0x03, 0x74, 0x62, 0x6c, 0x01, 0x70, 0x00, 0x01, //   env.tbl (table 1 funcref)
            0x03, 0x03, 0x02, 0x00, 0x00,                                           // 2 functions of type 0
            0x08, 0x01, 0x01,                                                       // start: func 1
            0x09, 0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00,                   // elem: table[0] = func 0
            0x0a, 5 + startBody.length, 0x02,
            0x02, 0x00, 0x0b,                                                       // func 0: valid nop
            startBody.length, ...startBody,                                         // func 1 (start)
            0x0b, 0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0xbb,                   // data: mem[0] = 0xbb
        ]);
    }

    assert.truthy(WebAssembly.validate(makeStartModule([0x00, 0x0b])));

    const invalidStartBytes = makeStartModule([0x00, 0x41, 0x00, 0x0b]);
    assert.falsy(WebAssembly.validate(invalidStartBytes));

    const mem = new WebAssembly.Memory({ initial: 1 });
    const view = new Uint8Array(mem.buffer);
    view[0] = 0xaa;
    const tbl = new WebAssembly.Table({ initial: 1, element: "anyfunc" });

    assert.throws(() => new WebAssembly.Module(invalidStartBytes), WebAssembly.CompileError, "in function at index 1");

    async function testInvalidStartInstantiate() {
        let caught;
        try {
            await WebAssembly.instantiate(invalidStartBytes, { env: { mem, tbl } });
        } catch (e) {
            caught = e;
        }
        assert.instanceof(caught, WebAssembly.CompileError);
        assert.truthy(caught.message.includes("in function at index 1"),
            `expected failing start function index 1, got: ${caught.message}`);
        assert.eq(view[0], 0xaa);
        assert.eq(tbl.get(0), null);
    }

    await assert.asyncTest(testInvalidStartInstantiate());
}
