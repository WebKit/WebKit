// Exercises the dupe-on-insert fallbacks in canonicalizeRecursionGroupImpl
// and canonicalizeSingletonImpl. When a just-parsed candidate module
// produces a rec-group or singleton structurally identical to one already
// in the canonical table, the candidate is discarded. Historically the
// discard path didn't break the candidate's self-display refs (and, for
// groups, its m_group back-refs), leaking the candidate forever.
//
// Two modules with the SAME type signature are constructed back-to-back.
// The second module's parse hits the dupe path. If dupe-on-insert leaks,
// the canonical table grows monotonically with iterations; with the fix,
// it stays bounded near baseline.

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
const REF_NULL = 0x63;
const TYPE_I32 = 0x7f;
const TYPE_I64 = 0x7e;
const FIELD_IMMUTABLE = 0x00;

// The uniqueness salt varies the shape per *pair*. Both modules in a pair
// share the same salt so their types canonicalize identically -> the second
// module's parse hits the dupe-on-insert path. Across pairs the salt varies
// so we don't merely dedup against a single canonical entry held by the
// first pair.
function buildMutualRecGroupBytes(pairSalt) {
    const low = pairSalt & 0xff;
    const high = (pairSalt >> 8) & 0xff;
    const i32Count = 1 + low;
    const i64Count = 1 + high;
    const totalStructFields = 1 + i32Count + i64Count;

    const structBody = [TYPE_STRUCT, ...uleb128(totalStructFields)];
    structBody.push(REF_NULL, ...sleb128(1), FIELD_IMMUTABLE);
    for (let i = 0; i < i32Count; ++i)
        structBody.push(TYPE_I32, FIELD_IMMUTABLE);
    for (let i = 0; i < i64Count; ++i)
        structBody.push(TYPE_I64, FIELD_IMMUTABLE);

    const arrayBody = [TYPE_ARRAY, REF_NULL, ...sleb128(0), FIELD_IMMUTABLE];

    const typeSectionBody = [
        0x01, TYPE_REC, 0x02,
        TYPE_SUB, 0x00, ...structBody,
        TYPE_SUB, 0x00, ...arrayBody,
    ];

    return new Uint8Array([
        ...WASM_MAGIC,
        ...WASM_VERSION,
        ...encodeSection(SEC_TYPE, typeSectionBody),
    ]);
}

// Self-recursive singleton (struct with ref to self). Same uniqueness logic.
function buildSelfRecursiveSingletonBytes(pairSalt) {
    const low = pairSalt & 0xff;
    const high = (pairSalt >> 8) & 0xff;
    const i32Count = 1 + low;
    const i64Count = 1 + high;
    const totalFields = 1 + i32Count + i64Count;

    const structBody = [TYPE_STRUCT, ...uleb128(totalFields)];
    structBody.push(REF_NULL, ...sleb128(0), FIELD_IMMUTABLE);
    for (let i = 0; i < i32Count; ++i)
        structBody.push(TYPE_I32, FIELD_IMMUTABLE);
    for (let i = 0; i < i64Count; ++i)
        structBody.push(TYPE_I64, FIELD_IMMUTABLE);

    const typeSectionBody = [
        0x01, TYPE_REC, 0x01,
        TYPE_SUB, 0x00, ...structBody,
    ];

    return new Uint8Array([
        ...WASM_MAGIC,
        ...WASM_VERSION,
        ...encodeSection(SEC_TYPE, typeSectionBody),
    ]);
}

if (typeof $vm === "undefined" || typeof $vm.wasmCanonicalTypeCount !== "function")
    throw new Error("$vm.wasmCanonicalTypeCount() is required for this test");

function churnPairs(builder, count, saltBase) {
    for (let i = 0; i < count; ++i) {
        const salt = saltBase + i;
        // First module: NEW canonical entry.
        const m1 = new WebAssembly.Module(builder(salt));
        // Second module with identical type: dupe-on-insert path.
        const m2 = new WebAssembly.Module(builder(salt));
        void m1;
        void m2;
    }
}

function verifyReclamation(label, builder) {
    const baseline = $vm.wasmCanonicalTypeCount();
    const pairs = 200;
    churnPairs(builder, pairs, 0);

    // Drive GC to quiescence via gc() (sync full + sweep).
    let after = $vm.wasmCanonicalTypeCount();
    for (let round = 0; round < 20; ++round) {
        const before = after;
        if (typeof gc === "function") gc();
        else if (typeof fullGC === "function") fullGC();
        after = $vm.wasmCanonicalTypeCount();
        if (after === before) break;
    }

    const grew = after - baseline;
    const budget = Math.floor(pairs / 10);
    if (grew > budget) {
        throw new Error(
            `[${label}] canonical table grew by ${grew} entries after ${pairs} dupe pairs ` +
            `(baseline=${baseline}, after=${after}, budget=${budget}). ` +
            `The dupe-on-insert path is probably leaking.`);
    }
}

verifyReclamation("mutual rec-group", buildMutualRecGroupBytes);
verifyReclamation("self-recursive singleton", buildSelfRecursiveSingletonBytes);
