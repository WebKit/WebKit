import * as assert from "../assert.js";
import { watToWasm } from "../gc/wast-wrapper.js";

// Regression test: br_on_cast must reject flags bytes with reserved bits set.
//
// Bug: the validator only extracts bits 0-1 from the flags byte without
// rejecting bits 2-7. The IPInt interpreter reloads the raw flags byte and
// computes allowNull = (flags >> 1), so setting bit 2 makes the runtime
// treat a non-null target cast as nullable. This allows null to be typed as
// a non-null reference — a type-system violation.

// Compile a valid module with br_on_cast, then patch the flags byte to 0x05.
// Valid flags=0x01 means: source nullable, target non-null.
// Patched flags=0x05: same from validator's perspective (bits 0-1 unchanged in
// the way that matters), but bit 2 set causes IPInt to derive allowNull=true.

let wat = `
(module
  (type (struct))
  (func (export "test") (result i32)
    (block (result (ref 0))
      (ref.null none)
      (br_on_cast 0 (ref null any) (ref 0))
      (drop)
      (struct.new 0)
    )
    (ref.is_null)
  )
)
`;

let binary = watToWasm(wat);
let bytes = new Uint8Array(binary);

// Find the br_on_cast opcode (0xFB 0x18) and patch its flags byte.
// The flags byte immediately follows the two-byte opcode: [0xFB, 0x18, flags, ...]
let patched = false;
for (let i = 0; i < bytes.length - 2; i++) {
    if (bytes[i] === 0xFB && bytes[i + 1] === 0x18) {
        let flagsOffset = i + 2;
        assert.eq(bytes[flagsOffset], 0x01, "expected valid flags=0x01 (src nullable, tgt non-null)");
        bytes[flagsOffset] = 0x05; // set reserved bit 2
        patched = true;
        break;
    }
}
assert.truthy(patched, "should have found br_on_cast opcode in binary");

// The module should be REJECTED because reserved bits are set.
assert.throws(
    () => new WebAssembly.Module(binary),
    WebAssembly.CompileError,
    "reserved bits"
);
