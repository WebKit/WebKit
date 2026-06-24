//@ $skipModes << :lockdown
//@ skip if $addressBits <= 32
// Benchmarks the wasm:js-string intoCharCodeArray builtin with an 8-bit string.
/*
(module
  (type $a (array (mut i16)))
  (import "wasm:js-string" "intoCharCodeArray" (func $into (param externref (ref null $a) i32) (result i32)))
  (func (export "makeArray") (param i32) (result (ref $a))
    (array.new_default $a (local.get 0)))
  (func (export "run") (param $s externref) (param $arr (ref null $a)) (param $iters i32) (result i32)
    (local $i i32) (local $sum i32)
    (block $done
      (loop $l
        (br_if $done (i32.ge_u (local.get $i) (local.get $iters)))
        (local.set $sum (i32.add (local.get $sum) (call $into (local.get $s) (local.get $arr) (i32.const 0))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $l)))
    (local.get $sum))
)
*/
const moduleBytes = new Uint8Array([0,97,115,109,1,0,0,0,1,146,128,128,128,0,3,94,119,1,96,3,111,99,0,127,1,127,96,1,127,1,100,0,2,164,128,128,128,0,1,14,119,97,115,109,58,106,115,45,115,116,114,105,110,103,17,105,110,116,111,67,104,97,114,67,111,100,101,65,114,114,97,121,0,1,3,131,128,128,128,0,2,2,1,7,147,128,128,128,0,2,9,109,97,107,101,65,114,114,97,121,0,1,3,114,117,110,0,2,10,187,128,128,128,0,2,135,128,128,128,0,0,32,0,251,7,0,11,169,128,128,128,0,1,2,127,2,64,3,64,32,3,32,2,79,13,1,32,4,32,0,32,1,65,0,16,0,106,33,4,32,3,65,1,106,33,3,12,0,11,11,32,4,11]);

const module = new WebAssembly.Module(moduleBytes, { builtins: ['js-string'] });
const instance = new WebAssembly.Instance(module, {});
const { makeArray, run } = instance.exports;

const length = 1024;
let string = "";
for (let i = 0; i < length; ++i)
    string += String.fromCharCode(0x20 + (i & 0x5f));
const array = makeArray(length);

const innerIterations = 1000;
let sum = 0;
for (let i = 0; i < 100; ++i)
    sum += run(string, array, innerIterations);

if (sum !== 100 * innerIterations * length)
    throw new Error("Bad result: " + sum);
