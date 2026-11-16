// globalThis.print = function (message) { }

function uleb128(value) {
    const result = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value !== 0) byte |= 0x80;
        result.push(byte);
    } while (value !== 0);
    return result;
}

function sleb128(value) {
    const result = [];
    let more = true;
    while (more) {
        let byte = value & 0x7f;
        value >>= 7;
        if ((value === 0 && (byte & 0x40) === 0) ||
            (value === -1 && (byte & 0x40) !== 0)) {
            more = false;
        } else {
            byte |= 0x80;
        }
        result.push(byte);
    }
    return result;
}

function encodeSection(id, contents) {
    return [id, ...uleb128(contents.length), ...contents];
}

function encodeString(str) {
    const bytes = [];
    for (let i = 0; i < str.length; i++) bytes.push(str.charCodeAt(i));
    return [...uleb128(bytes.length), ...bytes];
}

// Wasm opcodes
const WASM_MAGIC = [0x00, 0x61, 0x73, 0x6d];
const WASM_VERSION = [0x01, 0x00, 0x00, 0x00];

// Section IDs
const SEC_TYPE = 1;
const SEC_FUNCTION = 3;
const SEC_EXPORT = 7;
const SEC_CODE = 10;

// Type encodings
const TYPE_I32 = 0x7f;
const TYPE_I64 = 0x7e;
const TYPE_FUNCTYPE = 0x60;

// GC type encodings
const TYPE_STRUCT = 0x5f;       // struct
const TYPE_SUB = 0x50;          // sub (subtype, open)
const TYPE_REC = 0x4e;          // rec group

// Reference type encodings
const REF_NULL = 0x6c;          // (ref null $type) - nullable
const REF = 0x6b;               // (ref $type) - non-nullable

// Field mutability
const FIELD_IMMUTABLE = 0x00;
const FIELD_MUTABLE = 0x01;

// GC opcodes (0xfb prefix)
const GC_PREFIX = 0xfb;
const GC_STRUCT_NEW = 0x00;     // struct.new $type
const GC_STRUCT_GET = 0x02;     // struct.get $type $field
const GC_REF_TEST = 0x14;      // ref.test (ref ht) - non-nullable
const GC_REF_TEST_NULL = 0x15; // ref.test (ref null ht) - nullable
const GC_REF_CAST = 0x16;      // ref.cast (ref ht) - non-nullable
const GC_REF_CAST_NULL = 0x17; // ref.cast (ref null ht) - nullable

// Other opcodes
const OP_END = 0x0b;
const OP_LOCAL_GET = 0x20;
const OP_I32_CONST = 0x41;
const OP_I64_CONST = 0x42;

// Export kinds
const EXPORT_FUNC = 0x00;

// === Build the Module ===

function buildModule() {
    // Type section: rec group with Parent and Child struct types
    //
    // (rec
    //   (type $Parent (struct (field i32)))           ;; type 0
    //   (type $Child (sub $Parent (struct (field i32) (field i64))))  ;; type 1
    // )

    const typeSection = [
        0x01,       // 1 rec group

        TYPE_REC,   // rec group marker
        0x02,       // 2 types in the group

        // Type 0: $Parent = struct { field i32 mutable }
        TYPE_STRUCT,
        0x01,       // 1 field
        TYPE_I32, FIELD_MUTABLE,

        // Type 1: $Child = sub $Parent, struct { field i32 mutable, field i64 mutable }
        TYPE_SUB,
        0x01,       // 1 supertype
        ...uleb128(0), // supertype index 0 ($Parent)
        TYPE_STRUCT,
        0x02,       // 2 fields
        TYPE_I32, FIELD_MUTABLE,
        TYPE_I64, FIELD_MUTABLE,
    ];

    // Function types (we also need func types for our exported functions)
    // Type 2: (i32) -> (i32)
    // Type 3: () -> (i32)
    // Type 4: (i32) -> (i64)
    const funcTypes = [
        TYPE_FUNCTYPE, 0x01, TYPE_I32, 0x01, TYPE_I32,   // type 2: (i32) -> (i32)
        TYPE_FUNCTYPE, 0x00, 0x01, TYPE_I32,              // type 3: () -> (i32)
        TYPE_FUNCTYPE, 0x01, TYPE_I32, 0x01, TYPE_I64,   // type 4: (i32) -> (i64)
    ];

    // Combine type section: rec group + standalone func types
    const fullTypeSection = [
        0x01 + 0x03, // total: 1 rec group + 3 standalone types... but encoding is different
    ];

    // Actually, the type section count is the number of top-level entries.
    // A rec group counts as 1 entry. Then each standalone type also counts as 1.
    // So: 1 (rec group) + 3 (func types) = 4 entries
    const typeSectionBody = [
        0x04,       // 4 entries

        // Entry 1: rec group
        TYPE_REC,
        0x02,       // 2 types in rec group

        // Type 0: $Parent = sub (open, 0 supertypes) struct { i32 mutable }
        // Must use sub wrapper to make it non-final so Child can extend it
        TYPE_SUB,
        0x00,       // 0 supertypes (but open for subtyping)
        TYPE_STRUCT,
        0x01,       // 1 field
        TYPE_I32, FIELD_MUTABLE,

        // Type 1: $Child = sub $Parent, struct { i32 mutable, i64 mutable }
        TYPE_SUB,
        0x01,       // 1 supertype
        ...uleb128(0), // supertype 0
        TYPE_STRUCT,
        0x02,       // 2 fields
        TYPE_I32, FIELD_MUTABLE,
        TYPE_I64, FIELD_MUTABLE,

        // Entry 2: func type (i32) -> (i32)
        TYPE_FUNCTYPE, 0x01, TYPE_I32, 0x01, TYPE_I32,

        // Entry 3: func type () -> (i32)
        TYPE_FUNCTYPE, 0x00, 0x01, TYPE_I32,

        // Entry 4: func type (i32) -> (i64)
        TYPE_FUNCTYPE, 0x01, TYPE_I32, 0x01, TYPE_I64,
    ];

    // Function section: declare 4 functions
    // func 0: test_downcast  - type 2 (i32)->i32
    // func 1: test_upcast    - type 3 ()->i32
    // func 2: cast_downcast  - type 2 (i32)->i32
    // func 3: oob_read       - type 4 (i32)->i64
    const funcSectionBody = [
        0x04,       // 4 functions
        0x02,       // func 0 uses type index 2
        0x03,       // func 1 uses type index 3
        0x02,       // func 2 uses type index 2
        0x04,       // func 3 uses type index 4
    ];

    // Export section
    const exportSectionBody = [
        0x04,       // 4 exports
        ...encodeString("test_downcast"), EXPORT_FUNC, 0x00,
        ...encodeString("test_upcast"),   EXPORT_FUNC, 0x01,
        ...encodeString("cast_downcast"), EXPORT_FUNC, 0x02,
        ...encodeString("oob_read"),      EXPORT_FUNC, 0x03,
    ];

    // Code section: function bodies

    // func 0: test_downcast(val: i32) -> i32
    // (ref.test (ref $Child) (struct.new $Parent (local.get 0)))
    const func0Body = [
        0x00,                           // 0 local declarations
        OP_LOCAL_GET, 0x00,             // local.get 0
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(0),  // struct.new $Parent (type 0)
        GC_PREFIX, GC_REF_TEST, ...sleb128(1),    // ref.test (ref $Child) heap type 1
        OP_END,
    ];

    // func 1: test_upcast() -> i32
    // (ref.test (ref $Parent) (struct.new $Child (i32.const 42) (i64.const 99)))
    const func1Body = [
        0x00,                           // 0 local declarations
        OP_I32_CONST, ...sleb128(42),   // i32.const 42
        OP_I64_CONST, ...sleb128(99),   // i64.const 99
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(1),  // struct.new $Child (type 1)
        GC_PREFIX, GC_REF_TEST, ...sleb128(0),    // ref.test (ref $Parent) heap type 0
        OP_END,
    ];

    // func 2: cast_downcast(val: i32) -> i32
    // (struct.get $Child 0 (ref.cast (ref $Child) (struct.new $Parent (local.get 0))))
    const func2Body = [
        0x00,                           // 0 local declarations
        OP_LOCAL_GET, 0x00,             // local.get 0
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(0),  // struct.new $Parent (type 0)
        GC_PREFIX, GC_REF_CAST, ...sleb128(1),    // ref.cast (ref $Child) heap type 1
        GC_PREFIX, GC_STRUCT_GET, ...uleb128(1), ...uleb128(0),  // struct.get $Child field 0
        OP_END,
    ];

    // func 3: oob_read(val: i32) -> i64
    // (struct.get $Child 1 (ref.cast (ref $Child) (struct.new $Parent (local.get 0))))
    const func3Body = [
        0x00,                           // 0 local declarations
        OP_LOCAL_GET, 0x00,             // local.get 0
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(0),  // struct.new $Parent (type 0)
        GC_PREFIX, GC_REF_CAST, ...sleb128(1),    // ref.cast (ref $Child) heap type 1
        GC_PREFIX, GC_STRUCT_GET, ...uleb128(1), ...uleb128(1),  // struct.get $Child field 1
        OP_END,
    ];

    function encodeBody(body) {
        return [...uleb128(body.length), ...body];
    }

    const codeSectionBody = [
        0x04,       // 4 function bodies
        ...encodeBody(func0Body),
        ...encodeBody(func1Body),
        ...encodeBody(func2Body),
        ...encodeBody(func3Body),
    ];

    // Assemble the module
    const module = [
        ...WASM_MAGIC,
        ...WASM_VERSION,
        ...encodeSection(SEC_TYPE, typeSectionBody),
        ...encodeSection(SEC_FUNCTION, funcSectionBody),
        ...encodeSection(SEC_EXPORT, exportSectionBody),
        ...encodeSection(SEC_CODE, codeSectionBody),
    ];

    return new Uint8Array(module);
}

// === Main Test ===

function main() {
    let bytes;
    try {
        bytes = buildModule();
    } catch(e) {
        return;
    }

    let module, instance;
    try {
        module = new WebAssembly.Module(bytes);
        instance = new WebAssembly.Instance(module);
    } catch(e) {
        return;
    }

    // Test 1: ref.test downcast (should be 0)

    let interpResult = instance.exports.test_downcast(42);

    // Warmup for OMG
    for (let i = 0; i < 200000; i++) {
        instance.exports.test_downcast(i);
    }

    instance.exports.test_downcast(0x41414141);

    // Test 2: ref.test upcast (control, should be 1)

    for (let i = 0; i < 200000; i++) {
        instance.exports.test_upcast();
    }
    instance.exports.test_upcast();

    // Test 3: ref.cast downcast (should trap)
    let castBugTriggered = false;
    for (let i = 0; i < 200000; i++) {
        try {
            let r = instance.exports.cast_downcast(i);
            // If we get here, the cast didn't trap!
            if (i > 100) {
                castBugTriggered = true;
                $vm.abort();
                break;
            }
        } catch(e) {
            // Expected: trap
            continue;
        }
    }

    if (!castBugTriggered) {
        try {
            let r = instance.exports.cast_downcast(0x41414141);
            castBugTriggered = true;
            $vm.abort();
        } catch(e) {
        }
    }
}

main();
