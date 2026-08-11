// Exercises the cycle collector's sweep ordering for a multi-member rec-group
// that contains intra-group subtyping. This is the scenario where sweep must
// not interleave clearDisplayRefsToMembersOf with setCanonicalGroup(nullptr):
// the intra-group predicate `slot->canonicalGroup() == &deadGroup` must see
// every target's original m_group back-pointer, or display slots pointing at
// already-unlinked members get missed and dangle through ~RTT destruction.
//
// Shape (varied per iteration so each module gets a fresh canonical group):
//   (rec
//     (type $A (sub (struct (field i32))))
//     (type $B (sub $A (struct (field i32) (field (ref null $A))))))
// $B is a subtype of $A -- B's display contains A. Both are in the same
// rec-group. After module drop, both should be reclaimable.

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
const TYPE_SUB = 0x50;
const TYPE_REC = 0x4e;
const REF_NULL = 0x63;
const TYPE_I32 = 0x7f;
const TYPE_I64 = 0x7e;
const FIELD_IMMUTABLE = 0x00;

function buildModuleBytes(iterationSalt) {
    // Two-dimensional salt so each iteration produces a structurally
    // distinct rec-group (no dedup masking).
    const lowSalt = iterationSalt & 0xff;
    const highSalt = (iterationSalt >> 8) & 0xff;
    const i32Count = 1 + lowSalt;
    const i64Count = 1 + highSalt;

    // Type 0: $A = sub (open) struct { i32*i32Count, i64*i64Count }
    const aFieldCount = i32Count + i64Count;
    const aBody = [TYPE_STRUCT, ...uleb128(aFieldCount)];
    for (let i = 0; i < i32Count; ++i)
        aBody.push(TYPE_I32, FIELD_IMMUTABLE);
    for (let i = 0; i < i64Count; ++i)
        aBody.push(TYPE_I64, FIELD_IMMUTABLE);

    // Type 1: $B = sub $A struct { ..., (ref null $A) }
    //   B has A in its display chain; B's last field is a back-ref into the
    //   same rec-group's type 0 (intra-group ref => TypeSlot anchors cycle).
    const bFieldCount = i32Count + i64Count + 1;
    const bBody = [TYPE_STRUCT, ...uleb128(bFieldCount)];
    for (let i = 0; i < i32Count; ++i)
        bBody.push(TYPE_I32, FIELD_IMMUTABLE);
    for (let i = 0; i < i64Count; ++i)
        bBody.push(TYPE_I64, FIELD_IMMUTABLE);
    bBody.push(REF_NULL, ...sleb128(0), FIELD_IMMUTABLE);

    const typeSectionBody = [
        0x01,                         // 1 top-level entry (the rec group)
        TYPE_REC,
        0x02,                         // 2 types inside the rec group
        TYPE_SUB, 0x00, ...aBody,     // A: 0 supertypes
        TYPE_SUB, 0x01, ...sleb128(0), ...bBody, // B: 1 supertype = type 0 (A)
    ];

    return new Uint8Array([
        ...WASM_MAGIC,
        ...WASM_VERSION,
        ...encodeSection(SEC_TYPE, typeSectionBody),
    ]);
}

function churn(count, saltBase) {
    for (let i = 0; i < count; ++i) {
        const m = new WebAssembly.Module(buildModuleBytes(saltBase + i));
        void m;
    }
}

const batches = 10;
const perBatch = 300;
for (let b = 0; b < batches; ++b) {
    churn(perBatch, b * perBatch);
    if (typeof gc === "function")
        gc();
    else if (typeof fullGC === "function")
        fullGC();
}
