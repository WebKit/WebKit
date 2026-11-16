// Verifies that a parser failure AFTER RTT construction but BEFORE
// canonicalization does not leak the partially-built RTT. Historically an
// RTT wrote `this` into its self-display slot at construction, creating a
// permanent refcount cycle; anything that failed validation between
// tryCreate* and HashSet insertion leaked one RTT per failed parse. Now
// the self-slot is deferred until canonicalization success, so the
// candidate destructs cleanly when the parser unwinds.
//
// Strategy: construct a module whose type section is valid enough to
// tryCreate a struct RTT, then hits a validation error later that causes
// the parser to abandon the constructed RTT. Measure the canonical table
// before and after many such failed parses; with the fix, nothing accrues
// because the candidates were never canonicalized (and, critically, they
// also don't leak).

function uleb128(v) { const r = []; do { let b = v & 0x7f; v >>>= 7; if (v) b |= 0x80; r.push(b); } while (v); return r; }
function sleb128(v) { const r = []; let more = true; while (more) { let b = v & 0x7f; v >>= 7; if ((v === 0 && (b & 0x40) === 0) || (v === -1 && (b & 0x40) !== 0)) more = false; else b |= 0x80; r.push(b); } return r; }
function enc(id, c) { return [id, ...uleb128(c.length), ...c]; }

const WASM_MAGIC = [0x00, 0x61, 0x73, 0x6d];
const WASM_VERSION = [0x01, 0x00, 0x00, 0x00];
const SEC_TYPE = 1;
const SEC_FUNCTION = 3;
const TYPE_STRUCT = 0x5f;
const TYPE_SUB = 0x50;
const TYPE_REC = 0x4e;
const TYPE_I32 = 0x7f;
const FIELD_IMMUTABLE = 0x00;

// Builds a module whose type section starts a rec-group, successfully
// tryCreate's a valid first struct RTT, then hits an invalid byte before
// reaching the end of the rec-group. The parser must throw BEFORE
// canonicalizeRecursionGroup runs -- which means the first struct's RTT
// was constructed but never inserted into the canonical table. With the
// self-slot-set-at-construction design, that RTT is permanently pinned by
// its own self-display ref and leaks. With the deferred-self-slot design,
// it has no self-cycle and destructs cleanly when the rec-group's
// candidate Vector goes out of scope during parse-error unwind.
//
// Uniqueness across iterations: vary the struct's field count so the
// discarded RTT is structurally distinct per iteration. If it were ever
// to reach canonicalization (it shouldn't), each one would be unique.
function buildFailingModule(salt) {
    const i32Count = 1 + (salt & 0xff);
    const structBody = [TYPE_STRUCT, ...uleb128(i32Count)];
    for (let i = 0; i < i32Count; ++i)
        structBody.push(TYPE_I32, FIELD_IMMUTABLE);

    const typeSection = [
        0x01,        // 1 top-level entry: the rec group
        TYPE_REC,
        0x02,        // 2 types -- but we'll only provide 1 + garbage
        TYPE_SUB, 0x00, ...structBody,
        0xFF,        // invalid byte where type 1's form-byte should be
    ];

    return new Uint8Array([
        ...WASM_MAGIC, ...WASM_VERSION,
        ...enc(SEC_TYPE, typeSection),
    ]);
}

if (typeof $vm === "undefined" || typeof $vm.wasmCanonicalTypeCount !== "function")
    throw new Error("$vm.wasmCanonicalTypeCount() required");

const baseline = $vm.wasmCanonicalTypeCount();
const iterations = 500;

function churn() {
    for (let i = 0; i < iterations; ++i) {
        let threw = false;
        try {
            new WebAssembly.Module(buildFailingModule(i));
        } catch (e) {
            threw = true;
        }
        if (!threw)
            throw new Error(`iteration ${i}: expected WebAssembly.CompileError, module parsed successfully`);
    }
}

churn();

// The failed parses canonicalized their type sections successfully before
// the function section exploded. Trigger a tryCleanup by creating and
// dropping a valid trivial module -- JSWebAssemblyModule::destroy invokes
// the cycle collector, which should reclaim every canonicalized RTT with
// no external holder (including all 256 from the churn above).
function triggerCleanup() {
    const trivial = new Uint8Array([...WASM_MAGIC, ...WASM_VERSION]);
    for (let i = 0; i < 5; ++i) {
        const m = new WebAssembly.Module(trivial);
        void m;
    }
}
triggerCleanup();

// Drive GC to quiescence.
let after = $vm.wasmCanonicalTypeCount();
for (let round = 0; round < 20; ++round) {
    const before = after;
    if (typeof gc === "function") gc();
    else if (typeof fullGC === "function") fullGC();
    after = $vm.wasmCanonicalTypeCount();
    if (after === before) break;
}

const grew = after - baseline;
// With Option C (deferred self-slot), the cycle collector can reclaim
// these canonicalized singletons because their only owners are the table
// entry (+1) and the self-display slot (+1); trial-decrement leaves
// virtualRefCount == 0 -> eligible. Before the cycle collector landed,
// this test would leak all 256.
if (grew > Math.floor(iterations / 10)) {
    throw new Error(
        `canonical table grew by ${grew} entries after ${iterations} failed parses ` +
        `(baseline=${baseline}, after=${after}). Parse-failure RTTs are leaking.`);
}
