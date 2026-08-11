// Exercises the cycle collector in Wasm::TypeInformation::tryCleanup for
// multi-member recursion groups whose members mutually reference each other
// via TypeSlot::rttAnchor. Before the collector landed, these leaked on
// every module drop because hasOneRef() can never fire on a rec-group whose
// members form an intra-group cycle.
//
// Each iteration builds a unique module whose type section is one rec-group
// of the form:
//   (rec
//     (type $s (struct (field (ref null $a)) i32 ... i32))   ; iteration-specific field count
//     (type $a (array  (ref null $s))))
//
// Varying the struct's trailing i32 field count per iteration forces a
// fresh canonical rec-group entry each time. We churn many such modules
// under repeated full GCs; if the collector fails, the canonical table
// grows without bound and this test OOMs or times out. On success, the
// jsc process stays within a reasonable memory budget.

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

const WASM_MAGIC = [0x00, 0x61, 0x73, 0x6d];
const WASM_VERSION = [0x01, 0x00, 0x00, 0x00];

const SEC_TYPE = 1;
const TYPE_STRUCT = 0x5f;
const TYPE_ARRAY = 0x5e;
const TYPE_SUB = 0x50;
const TYPE_REC = 0x4e;
const REF_NULL = 0x63; // (ref null ht) prefix; heap type follows as varint32
const TYPE_I32 = 0x7f;
const TYPE_I64 = 0x7e;
const FIELD_IMMUTABLE = 0x00;

function buildModuleBytes(iterationSalt) {
    // 2-dimensional salt: low 8 bits drive i32-field count, next 8 bits
    // drive i64-field count. Yields up to 256*256 structurally-unique
    // rec-groups so canonicalization dedup can't bound the table and mask
    // a broken collector.
    const lowSalt = iterationSalt & 0xff;
    const highSalt = (iterationSalt >> 8) & 0xff;
    const i32Count = 1 + lowSalt;
    const i64Count = 1 + highSalt;
    const totalStructFields = 1 + i32Count + i64Count;

    const structBody = [TYPE_STRUCT, ...uleb128(totalStructFields)];
    structBody.push(REF_NULL, ...sleb128(1), FIELD_IMMUTABLE);
    for (let i = 0; i < i32Count; ++i)
        structBody.push(TYPE_I32, FIELD_IMMUTABLE);
    for (let i = 0; i < i64Count; ++i)
        structBody.push(TYPE_I64, FIELD_IMMUTABLE);

    const arrayBody = [TYPE_ARRAY, REF_NULL, ...sleb128(0), FIELD_IMMUTABLE];

    const typeSectionBody = [
        0x01,           // 1 top-level entry (the rec group itself)
        TYPE_REC,
        0x02,           // 2 types inside the rec group
        TYPE_SUB, 0x00, ...structBody, // struct @ 0, no supertypes
        TYPE_SUB, 0x00, ...arrayBody,  // array @ 1, no supertypes
    ];

    return new Uint8Array([
        ...WASM_MAGIC,
        ...WASM_VERSION,
        ...encodeSection(SEC_TYPE, typeSectionBody),
    ]);
}

function churnModules(count, saltBase) {
    for (let i = 0; i < count; ++i) {
        const bytes = buildModuleBytes(saltBase + i);
        const m = new WebAssembly.Module(bytes);
        // Drop the strong ref to m immediately; only the intra-rec-group
        // TypeSlot anchor cycle would keep the canonical entry alive if the
        // collector fails.
        void m;
    }
}

// Use gc() (full + sync sweep) rather than fullGC() (full + deferred
// sweep) so JSWebAssemblyModule::destroy -- which calls tryCleanup --
// actually fires inside each GC call.
const batches = 20;
const perBatch = 500;
for (let b = 0; b < batches; ++b) {
    churnModules(perBatch, b * perBatch);
    if (typeof gc === "function")
        gc();
    else if (typeof fullGC === "function")
        fullGC();
}
