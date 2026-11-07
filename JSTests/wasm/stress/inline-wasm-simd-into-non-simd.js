load("../libwabt.js");

const NUM_REFS = 49;

let wabt = await WabtModule();

let refParams = "", refResults = "", pushResults = "";

for (let i = 1; i <= NUM_REFS; i++) {
    refParams  += " externref";
    refResults += " externref";
    pushResults += `(local.get ${i})\n`;
}

const wat = `
(module
  (tag $T (param i64 i64 i64 v128))
  (func $B (param i64)
    (throw $T (i64.const 0) (i64.const 0) (i64.const 0)
              (i64x2.splat (local.get 0))))
  (func $A (export "A") (param i64${refParams}) (result${refResults})
    (try
      (do (call $B (local.get 0)))
      (catch_all))
    ${pushResults}
  )
)`;

const bin = wabt.parseWat("filenamesAreCool", wat, { simd: true, exceptions: true, reference_types: true, multi_value: true }).toBinary({}).buffer;
const inst = new WebAssembly.Instance(new WebAssembly.Module(bin), {});
const A = inst.exports.A;

const sentinel = { marker: "SENTINEL" };
const args = [0xan];
for (let i = 0; i < NUM_REFS; i++)
    args.push(sentinel);

// Tier $A up to OMG.
let last;
for (let i = 0; i < testLoopCount; i++)
    last = A.apply(null, args);

let bad = -1;

for (let k = 0; k < NUM_REFS; k++) {
    if (last[k] !== sentinel) {
        bad = k;
        break;
    }
}

if (bad >= 0) {
    throw new Error("usesSIMD desync");
}
