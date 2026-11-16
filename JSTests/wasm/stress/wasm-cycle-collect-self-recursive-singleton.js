// Exercises the cycle collector in Wasm::TypeInformation::tryCleanup for
// self-recursive singletons (rec-group of size 1 whose single type references
// itself). Before the collector landed, the existing `hasOneRef()` predicate
// in tryCleanup couldn't fire on such a singleton because the TypeSlot's
// rttAnchor + the self-display slot each add a self-reference, keeping the
// refcount at >= 2.
//
// Each iteration builds a module of shape:
//   (rec (type $s (struct (field (ref null $s)) i32 ... i32)))
// The trailing i32 field count varies per iteration so every iteration
// produces a fresh canonical singleton.

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
    const lowSalt = iterationSalt & 0xff;
    const highSalt = (iterationSalt >> 8) & 0xff;
    const i32Count = 1 + lowSalt;
    const i64Count = 1 + highSalt;
    const totalFields = 1 + i32Count + i64Count;

    const structBody = [TYPE_STRUCT, ...uleb128(totalFields)];
    // Self-reference: (ref null 0) where 0 is this very type's module index.
    structBody.push(REF_NULL, ...sleb128(0), FIELD_IMMUTABLE);
    for (let i = 0; i < i32Count; ++i)
        structBody.push(TYPE_I32, FIELD_IMMUTABLE);
    for (let i = 0; i < i64Count; ++i)
        structBody.push(TYPE_I64, FIELD_IMMUTABLE);

    const typeSectionBody = [
        0x01,
        TYPE_REC,
        0x01, // 1 type inside rec
        TYPE_SUB, 0x00, ...structBody,
    ];

    return new Uint8Array([
        ...WASM_MAGIC,
        ...WASM_VERSION,
        ...encodeSection(SEC_TYPE, typeSectionBody),
    ]);
}

function churnModules(count, saltBase) {
    for (let i = 0; i < count; ++i) {
        const m = new WebAssembly.Module(buildModuleBytes(saltBase + i));
        void m;
    }
}

const batches = 20;
const perBatch = 500;
for (let b = 0; b < batches; ++b) {
    churnModules(perBatch, b * perBatch);
    if (typeof gc === "function")
        gc();
    else if (typeof fullGC === "function")
        fullGC();
}
