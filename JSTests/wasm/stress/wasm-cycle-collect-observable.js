// Directly verifies that Wasm::TypeInformation::tryCleanup actually reclaims
// canonical entries (as opposed to merely keeping memory bounded via
// canonicalization dedup). Uses the $vm.wasmCanonicalTypeCount() helper to
// observe the canonical table size before and after a batch of churn.
//
// Each iteration builds a structurally-UNIQUE rec-group, so there's no dedup
// fallback -- if the cycle collector doesn't run, the table will grow
// linearly with iterations. After a full GC, the table should drop back to
// near the baseline.

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

// Encode a 2-dimensional salt: low 8 bits drive i32-field count, next 8 bits
// drive i64-field count. Unique across (lowSalt, highSalt) space, up to 256
// * 256 combinations before wrapping.
function buildModuleBytes(iter) {
    const lowSalt = iter & 0xff;
    const highSalt = (iter >> 8) & 0xff;
    const i32Count = 1 + lowSalt;
    const i64Count = 1 + highSalt;

    // struct at index 0: 1 ref-to-array + i32Count i32 + i64Count i64
    const totalStructFields = 1 + i32Count + i64Count;
    const structBody = [TYPE_STRUCT, ...uleb128(totalStructFields)];
    structBody.push(REF_NULL, ...sleb128(1), FIELD_IMMUTABLE);
    for (let i = 0; i < i32Count; ++i)
        structBody.push(TYPE_I32, FIELD_IMMUTABLE);
    for (let i = 0; i < i64Count; ++i)
        structBody.push(TYPE_I64, FIELD_IMMUTABLE);

    // array at index 1: element = ref-to-struct
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

if (typeof $vm === "undefined" || typeof $vm.wasmCanonicalTypeCount !== "function")
    throw new Error("$vm.wasmCanonicalTypeCount() is required for this test");

const baseline = $vm.wasmCanonicalTypeCount();
const iterations = 500;

// Churn inside a helper function so every local `m` goes out of scope when
// the call returns -- stack / register conservative roots shouldn't pin any
// module past the return. Without this, the JS GC conservatively holds onto
// modules via in-frame references and fullGC() drains only a trickle per
// call.
function churnBatch(start, count) {
    for (let i = 0; i < count; ++i) {
        const m = new WebAssembly.Module(buildModuleBytes(start + i));
        void m;
    }
}

churnBatch(0, iterations);

// Drive GC to quiescence. `gc()` (= collectNow(Sync, Full)) is the right
// primitive here -- it runs a full collection AND sweeps synchronously, so
// JSWebAssemblyModule::destroy (and therefore TypeInformation::tryCleanup)
// actually fires inside the call. `fullGC()` alone runs the mark phase but
// defers sweeping, which is why earlier versions of this test saw only a
// handful of modules destroyed per call.
let after = $vm.wasmCanonicalTypeCount();
for (let round = 0; round < 20; ++round) {
    const before = after;
    if (typeof gc === "function")
        gc();
    else if (typeof fullGC === "function")
        fullGC();
    after = $vm.wasmCanonicalTypeCount();
    if (after === before)
        break;
}

const grew = after - baseline;

// With working reclamation, `grew` should be a small constant. Without
// reclamation, grew ~= iterations. Budget at one-tenth of iterations for a
// conservative middle ground that catches regressions while tolerating a
// few conservatively-rooted modules.
const budget = Math.floor(iterations / 10);
if (grew > budget) {
    throw new Error(
        `canonical table grew by ${grew} entries after churn of ${iterations} unique modules ` +
        `(baseline=${baseline}, after=${after}, budget=${budget}). ` +
        `This suggests Wasm::TypeInformation::tryCleanup is not reclaiming rec-groups.`);
}
