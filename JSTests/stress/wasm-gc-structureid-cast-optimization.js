// Tests for WasmRefTypeCheckValue with StructureID-based type checking.
// Exercises B3 ReduceStrength's replaceWithNullTrapping and ValueKey encoding
// for 2-child WasmRefTypeCheckValue nodes.

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

const WASM_MAGIC = [0x00, 0x61, 0x73, 0x6d];
const WASM_VERSION = [0x01, 0x00, 0x00, 0x00];

const SEC_TYPE = 1;
const SEC_FUNCTION = 3;
const SEC_EXPORT = 7;
const SEC_CODE = 10;

const TYPE_I32 = 0x7f;
const TYPE_I64 = 0x7e;
const TYPE_FUNCTYPE = 0x60;
const TYPE_STRUCT = 0x5f;
const TYPE_SUB = 0x50;
const TYPE_SUB_FINAL = 0x4f;
const TYPE_REC = 0x4e;
const REF_NULL = 0x6c;
const REF = 0x6b;
const FIELD_MUTABLE = 0x01;

const GC_PREFIX = 0xfb;
const GC_STRUCT_NEW = 0x00;
const GC_STRUCT_GET = 0x02;
const GC_REF_TEST = 0x14;
const GC_REF_TEST_NULL = 0x15;
const GC_REF_CAST = 0x16;
const GC_REF_CAST_NULL = 0x17;

const OP_END = 0x0b;
const OP_LOCAL_GET = 0x20;
const OP_LOCAL_SET = 0x21;
const OP_I32_CONST = 0x41;
const OP_I64_CONST = 0x42;
const OP_I32_ADD = 0x6a;
const OP_IF = 0x04;
const OP_ELSE = 0x05;
const OP_BLOCK = 0x02;
const OP_LOOP = 0x03;
const OP_BR = 0x0c;
const OP_BR_IF = 0x0d;
const OP_I32_LT_S = 0x48;
const OP_I32_EQZ = 0x45;
const OP_REF_NULL = 0xd0;
const OP_DROP = 0x1a;
const OP_RETURN = 0x0f;
const OP_REF_IS_NULL = 0xd1;
const EXPORT_FUNC = 0x00;

function buildModule() {
    // Type hierarchy:
    //   type 0: $Base = sub (open) struct { i32 }
    //   type 1: $Mid  = sub $Base (open) struct { i32, i64 }
    //   type 2: $Leaf = sub final $Mid struct { i32, i64, i32 }
    //   type 3: $Final = sub final struct { i32 }  (unrelated final type)
    //
    // Function types:
    //   type 4: (i32) -> (i32)
    //   type 5: () -> (i32)
    //   type 6: (i32) -> (i64)

    const typeSectionBody = [
        0x05,       // 5 entries: 1 rec group + 1 standalone struct + 3 func types

        // Entry 1: rec group with 3 types
        TYPE_REC,
        0x03,       // 3 types in rec group

        // Type 0: $Base = sub (open) struct { i32 mutable }
        TYPE_SUB,
        0x00,       // 0 supertypes
        TYPE_STRUCT,
        0x01,       // 1 field
        TYPE_I32, FIELD_MUTABLE,

        // Type 1: $Mid = sub $Base (open) struct { i32, i64 }
        TYPE_SUB,
        0x01,       // 1 supertype
        ...uleb128(0),
        TYPE_STRUCT,
        0x02,       // 2 fields
        TYPE_I32, FIELD_MUTABLE,
        TYPE_I64, FIELD_MUTABLE,

        // Type 2: $Leaf = sub final $Mid struct { i32, i64, i32 }
        TYPE_SUB_FINAL,
        0x01,       // 1 supertype
        ...uleb128(1),
        TYPE_STRUCT,
        0x03,       // 3 fields
        TYPE_I32, FIELD_MUTABLE,
        TYPE_I64, FIELD_MUTABLE,
        TYPE_I32, FIELD_MUTABLE,

        // Entry 2: $Final = sub final struct { i32 } (standalone, unrelated final type)
        TYPE_SUB_FINAL,
        0x00,       // 0 supertypes
        TYPE_STRUCT,
        0x01,
        TYPE_I32, FIELD_MUTABLE,

        // Entry 3: (i32) -> (i32)
        TYPE_FUNCTYPE, 0x01, TYPE_I32, 0x01, TYPE_I32,

        // Entry 4: () -> (i32)
        TYPE_FUNCTYPE, 0x00, 0x01, TYPE_I32,

        // Entry 5: (i32) -> (i64)
        TYPE_FUNCTYPE, 0x01, TYPE_I32, 0x01, TYPE_I64,
    ];

    const funcSectionBody = [
        0x07,       // 7 functions
        0x04,       // func 0: cast_to_final    type (i32)->i32
        0x04,       // func 1: cast_to_mid      type (i32)->i32
        0x04,       // func 2: test_final       type (i32)->i32
        0x04,       // func 3: test_mid         type (i32)->i32
        0x04,       // func 4: cast_to_parent   type (i32)->i32
        0x06,       // func 5: null_cast_get    type (i32)->i64
        0x05,       // func 6: loop_cast_final  type ()->i32
    ];

    const exportSectionBody = [
        0x07,
        ...encodeString("cast_to_final"),   EXPORT_FUNC, 0x00,
        ...encodeString("cast_to_mid"),     EXPORT_FUNC, 0x01,
        ...encodeString("test_final"),      EXPORT_FUNC, 0x02,
        ...encodeString("test_mid"),        EXPORT_FUNC, 0x03,
        ...encodeString("cast_to_parent"),  EXPORT_FUNC, 0x04,
        ...encodeString("null_cast_get"),   EXPORT_FUNC, 0x05,
        ...encodeString("loop_cast_final"), EXPORT_FUNC, 0x06,
    ];

    // func 0: cast_to_final(val: i32) -> i32
    // Create $Leaf, cast to $Leaf (final type => StructureID compare), get field 0
    const func0Body = [
        0x00,
        OP_LOCAL_GET, 0x00,
        OP_I64_CONST, ...sleb128(100),
        OP_LOCAL_GET, 0x00,
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(2),    // struct.new $Leaf
        GC_PREFIX, GC_REF_CAST, ...sleb128(2),      // ref.cast (ref $Leaf)
        GC_PREFIX, GC_STRUCT_GET, ...uleb128(2), ...uleb128(0),  // struct.get $Leaf field 0
        OP_END,
    ];

    // func 1: cast_to_mid(val: i32) -> i32
    // Create $Leaf, cast to $Mid (non-final, shallow hierarchy), get field 0
    const func1Body = [
        0x00,
        OP_LOCAL_GET, 0x00,
        OP_I64_CONST, ...sleb128(200),
        OP_LOCAL_GET, 0x00,
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(2),    // struct.new $Leaf
        GC_PREFIX, GC_REF_CAST, ...sleb128(1),      // ref.cast (ref $Mid)
        GC_PREFIX, GC_STRUCT_GET, ...uleb128(1), ...uleb128(0),  // struct.get $Mid field 0
        OP_END,
    ];

    // func 2: test_final(val: i32) -> i32
    // Create $Leaf, ref.test for $Leaf (final type)
    const func2Body = [
        0x00,
        OP_LOCAL_GET, 0x00,
        OP_I64_CONST, ...sleb128(300),
        OP_LOCAL_GET, 0x00,
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(2),    // struct.new $Leaf
        GC_PREFIX, GC_REF_TEST, ...sleb128(2),      // ref.test (ref $Leaf)
        OP_END,
    ];

    // func 3: test_mid(val: i32) -> i32
    // Create $Leaf, ref.test for $Mid (non-final)
    const func3Body = [
        0x00,
        OP_LOCAL_GET, 0x00,
        OP_I64_CONST, ...sleb128(400),
        OP_LOCAL_GET, 0x00,
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(2),    // struct.new $Leaf
        GC_PREFIX, GC_REF_TEST, ...sleb128(1),      // ref.test (ref $Mid)
        OP_END,
    ];

    // func 4: cast_to_parent(val: i32) -> i32
    // Create $Leaf, cast to $Base (subtype hierarchy), get field 0
    const func4Body = [
        0x00,
        OP_LOCAL_GET, 0x00,
        OP_I64_CONST, ...sleb128(500),
        OP_LOCAL_GET, 0x00,
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(2),    // struct.new $Leaf
        GC_PREFIX, GC_REF_CAST, ...sleb128(0),      // ref.cast (ref $Base)
        GC_PREFIX, GC_STRUCT_GET, ...uleb128(0), ...uleb128(0),  // struct.get $Base field 0
        OP_END,
    ];

    // func 5: null_cast_get(val: i32) -> i64
    // ref.cast null $Leaf (allowNull) + struct.get => triggers replaceWithNullTrapping in ReduceStrength
    // Always creates a valid $Leaf, but uses ref.cast null variant
    const func5Body = [
        0x00,                               // 0 local declarations
        OP_LOCAL_GET, 0x00,                 // local.get 0 (val)
        OP_I64_CONST, ...sleb128(777),
        OP_I32_CONST, ...sleb128(888),
        GC_PREFIX, GC_STRUCT_NEW, ...uleb128(2),  // struct.new $Leaf
        GC_PREFIX, GC_REF_CAST_NULL, ...sleb128(2),  // ref.cast (ref null $Leaf) - allowNull
        GC_PREFIX, GC_STRUCT_GET, ...uleb128(2), ...uleb128(1),  // struct.get $Leaf field 1
        OP_END,
    ];

    // func 6: loop_cast_final() -> i32
    // Loop doing ref.cast to final type to trigger OMG compilation
    // 2 locals: i32 counter, i32 sum
    const func6Body = [
        0x02,                               // 2 local declarations
        0x01, TYPE_I32,                     // 1 local of type i32 (counter)
        0x01, TYPE_I32,                     // 1 local of type i32 (sum)
        // counter = 0, sum = 0 (already zero-initialized)
        OP_BLOCK, 0x40,                     // block (void)
            OP_LOOP, 0x40,                  // loop (void)
                // Create $Leaf, cast to $Leaf (final), get field 0, add to sum
                OP_LOCAL_GET, 0x00,         // counter
                OP_I64_CONST, ...sleb128(0),
                OP_I32_CONST, ...sleb128(0),
                GC_PREFIX, GC_STRUCT_NEW, ...uleb128(2),    // struct.new $Leaf
                GC_PREFIX, GC_REF_CAST, ...sleb128(2),      // ref.cast (ref $Leaf)
                GC_PREFIX, GC_STRUCT_GET, ...uleb128(2), ...uleb128(0),
                OP_LOCAL_GET, 0x01,         // sum
                OP_I32_ADD,
                OP_LOCAL_SET, 0x01,         // sum += result
                // counter++
                OP_LOCAL_GET, 0x00,
                OP_I32_CONST, ...sleb128(1),
                OP_I32_ADD,
                OP_LOCAL_SET, 0x00,
                // if counter < 100000, continue loop
                OP_LOCAL_GET, 0x00,
                OP_I32_CONST, ...sleb128(100000),
                OP_I32_LT_S,
                OP_BR_IF, 0x00,             // br_if loop
            OP_END,                         // end loop
        OP_END,                             // end block
        OP_LOCAL_GET, 0x01,                 // return sum
        OP_END,
    ];

    function encodeBody(body) {
        return [...uleb128(body.length), ...body];
    }

    const codeSectionBody = [
        0x07,
        ...encodeBody(func0Body),
        ...encodeBody(func1Body),
        ...encodeBody(func2Body),
        ...encodeBody(func3Body),
        ...encodeBody(func4Body),
        ...encodeBody(func5Body),
        ...encodeBody(func6Body),
    ];

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

function main() {
    let bytes;
    try {
        bytes = buildModule();
    } catch (e) {
        throw new Error("Failed to build module: " + e);
    }

    let module, instance;
    try {
        module = new WebAssembly.Module(bytes);
        instance = new WebAssembly.Instance(module);
    } catch (e) {
        throw new Error("Failed to instantiate module: " + e);
    }

    const exports = instance.exports;

    // Case A: ref.cast to final type ($Leaf) — StructureID compare path
    for (let i = 0; i < 200000; i++) {
        let result = exports.cast_to_final(i);
        if (result !== i)
            throw new Error("cast_to_final(" + i + ") returned " + result + ", expected " + i);
    }

    // Case B: ref.cast to non-final type ($Mid) — inlined display path
    for (let i = 0; i < 200000; i++) {
        let result = exports.cast_to_mid(i);
        if (result !== i)
            throw new Error("cast_to_mid(" + i + ") returned " + result + ", expected " + i);
    }

    // Case C: ref.test for final type ($Leaf)
    for (let i = 0; i < 200000; i++) {
        let result = exports.test_final(i);
        if (result !== 1)
            throw new Error("test_final(" + i + ") returned " + result + ", expected 1");
    }

    // Case D: ref.test for non-final type ($Mid)
    for (let i = 0; i < 200000; i++) {
        let result = exports.test_mid(i);
        if (result !== 1)
            throw new Error("test_mid(" + i + ") returned " + result + ", expected 1");
    }

    // Case E: cast to parent type ($Base) from $Leaf
    for (let i = 0; i < 200000; i++) {
        let result = exports.cast_to_parent(i);
        if (result !== i)
            throw new Error("cast_to_parent(" + i + ") returned " + result + ", expected " + i);
    }

    // Case F: null handling — ref.cast null + struct.get triggers replaceWithNullTrapping
    // The ReduceStrength optimization converts ref.cast(allowNull) + struct.get to ref.cast(!allowNull) + struct.get
    for (let i = 0; i < 200000; i++) {
        let result = exports.null_cast_get(i);
        if (result !== 777n)
            throw new Error("null_cast_get(" + i + ") returned " + result + ", expected 777n");
    }

    // Case G: loop with repeated casts to trigger OMG
    for (let i = 0; i < 10; i++) {
        exports.loop_cast_final();
    }
}

main();
