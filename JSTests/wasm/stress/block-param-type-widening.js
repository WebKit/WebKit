import * as assert from "../assert.js";

// When a block/if/try/try_table declares a parameter type that is a *supertype*
// of the value actually on the stack, the block body must be validated against
// the DECLARED type, not the narrower concrete type that flowed in
// Each case below is built twice with an identical body:
//   - declared type = anyref (wide): `struct.get 0 0` is invalid on anyref, so
//     the module must be REJECTED. Before widening, the body saw the concrete
//     `(ref null 0)` and this was (incorrectly) accepted.
//   - declared type = (ref null 0) (concrete): `struct.get 0 0` is valid, so the
//     module must VALIDATE. This control confirms the scaffolding is otherwise
//     well-formed, so the rejection above is due to widening alone.

function uleb128(n) { const r = []; do { let b = n & 0x7f; n >>>= 7; if (n) b |= 0x80; r.push(b); } while (n); return r; }
function encodeString(s) { const b = []; for (let i = 0; i < s.length; i++) b.push(s.charCodeAt(i)); return [...uleb128(b.length), ...b]; }
function section(id, content) { return [id, ...uleb128(content.length), ...content]; }

// Module layout:
//   type 0: struct { i64 mut }
//   type 1: <blockSig>       (the signature of the block under test)
//   type 2: func () -> i64   (the exported function "test")
function buildModule(blockSig, body0) {
    const typeSection = section(1, [
        0x03,
        0x5F, 0x01, 0x7E, 0x01,          // type 0: struct { i64 mut }
        ...blockSig,                     // type 1
        0x60, 0x00, 0x01, 0x7E,          // type 2: () -> i64
    ]);
    const funcSection = section(3, [0x01, 0x02]);                               // func 0 : type 2
    const exportSection = section(7, [0x01, ...encodeString("test"), 0x00, 0x00]); // export "test" func 0
    const codeSection = section(10, [0x01, ...uleb128(body0.length), ...body0]);
    return new Uint8Array([0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...exportSection, ...codeSection]);
}

const sigParam = (t) => [0x60, 0x01, ...t, 0x01, 0x7E]; // (t) -> (i64)
const ANYREF = [0x6E];
const REF_NULL_0 = [0x63, 0x00]; // (ref null 0)

const GET = [0xFB, 0x02, 0x00, 0x00]; // struct.get 0 0
const REF_NULL_TYPE0 = [0xD0, 0x00];  // ref.null 0  -> (ref null 0)

// name -> { sig: (refTypeBytes) -> typeEntry, body }
const cases = {
    // (block (param T) (result i64) (struct.get 0 0))
    "block param": {
        sig: sigParam,
        body: [0x00, ...REF_NULL_TYPE0, 0x02, 0x01, ...GET, 0x0B, 0x0B],
    },
    // (if (param T) (result i64) (then struct.get 0 0) (else drop i64.const 0))
    "if param": {
        sig: sigParam,
        body: [0x00, ...REF_NULL_TYPE0, 0x41, 0x01, 0x04, 0x01, ...GET, 0x05, 0x1A, 0x42, 0x00, 0x0B, 0x0B],
    },
    // (try (param T) (result i64) (do struct.get 0 0) (catch_all i64.const 0))
    "try param": {
        sig: sigParam,
        body: [0x00, ...REF_NULL_TYPE0, 0x06, 0x01, ...GET, 0x19, 0x42, 0x00, 0x0B, 0x0B],
    },
    // (try_table (param T) (result i64) (struct.get 0 0))   -- 0 catch clauses
    "try_table param": {
        sig: sigParam,
        body: [0x00, ...REF_NULL_TYPE0, 0x1F, 0x01, 0x00, ...GET, 0x0B, 0x0B],
    },
};

for (const [name, { sig, body }] of Object.entries(cases)) {
    assert.falsy(WebAssembly.validate(buildModule(sig(ANYREF), body)),
        `${name}: struct.get on a widened anyref must be rejected (declared type not the concrete incoming type)`);
    assert.truthy(WebAssembly.validate(buildModule(sig(REF_NULL_0), body)),
        `${name}: struct.get on the concrete (ref null 0) must validate`);
}
