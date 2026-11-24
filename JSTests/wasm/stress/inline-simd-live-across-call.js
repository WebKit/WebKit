load("../libwabt.js");

const vectorCount = 24;
let locals = "";
let defs = "";
let adds = `(local.set $acc (local.get $v${vectorCount - 1}))\n`;
for (let i = 0; i < vectorCount; ++i) {
    locals += `(local $v${i} v128)\n`;
    defs += `(local.set $v${i} (f32x4.mul (v128.load offset=${4096 + i * 16} (i32.const 0)) (f32x4.splat (f32.convert_i32_s (i32.add (local.get $p) (i32.const ${i + 1}))))))\n`;
    if (i < vectorCount - 1)
        adds += `(local.set $acc (f32x4.add (local.get $acc) (local.get $v${vectorCount - 2 - i})))\n`;
}

const wat = `
(module
  (memory (export "memory") 1)
  (type $clobberType (func (param i32) (result i32)))
  (table 1 funcref)
  (elem (i32.const 0) $clobber)
  (func $storeIdentity (param $addr i32)
    (v128.store (local.get $addr) (v128.const f32x4 1 0 0 1)))
  (func $clobber (type $clobberType) (param $p i32) (result i32)
    ${locals}
    (local $acc v128)
    ${defs}
    ${adds}
    (i32.trunc_f32_s (f32x4.extract_lane 0 (local.get $acc))))
  (func (export "run") (param $base i32) (param $n i32)
    (local $i i32)
    (loop $next
      (drop (call_indirect (type $clobberType) (local.get $i) (i32.const 0)))
      (call $storeIdentity
        (i32.add (local.get $base) (i32.shl (local.get $i) (i32.const 4))))
      (br_if $next
        (i32.lt_u
          (local.tee $i (i32.add (local.get $i) (i32.const 1)))
          (local.get $n))))))
`;

let wabt = await WabtModule();
const bin = wabt.parseWat("inline-simd-live-across-call", wat, { simd: true }).toBinary({}).buffer;
const instance = new WebAssembly.Instance(new WebAssembly.Module(bin), {});
const memory = instance.exports.memory;
const run = instance.exports.run;

const slots = 4;
const inputs = new Float32Array(memory.buffer, 4096, vectorCount * 4);
for (let i = 0; i < inputs.length; ++i)
    inputs[i] = (i % 7) + 0.5;
const out = new Float32Array(memory.buffer, 0, slots * 4);

for (let call = 0; call < wasmTestLoopCount; ++call) {
    out.fill(0);
    run(0, slots);
    for (let slot = 0; slot < slots; ++slot) {
        const base = slot * 4;
        if (out[base] !== 1 || out[base + 1] !== 0 || out[base + 2] !== 0 || out[base + 3] !== 1)
            throw new Error(`call ${call} slot ${slot}: ${out[base]}, ${out[base + 1]}, ${out[base + 2]}, ${out[base + 3]}`);
    }
}
