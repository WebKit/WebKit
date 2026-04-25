import { instantiate as instantiateGC, watToWasm } from "../../../gc/wast-wrapper.js"

let wat = `
  (module
    (func (export "test") (param anyref) (result i32)
      (block (result i32 i31ref)
        (i32.const 42)
        (local.get 0)
        (br_on_cast 0 anyref i31ref)
        (br 0 (i32.const 7) (ref.null i31)))
      drop))
`

const binary = new Uint8Array(watToWasm(wat));
const inst = instantiateGC(wat);

print("Binary size:", binary.length, "bytes\n");

print("Binary (hex):");
let line = "";
for (let i = 0; i < binary.length; i++) {
    if (i % 16 === 0) {
        if (line) print(line);
        line = binary[i].toString(16).padStart(2, '0');
    } else {
        line += " " + binary[i].toString(16).padStart(2, '0');
    }
}
if (line) print(line);

print("\nTest: test(null) =", inst.exports.test(null));
