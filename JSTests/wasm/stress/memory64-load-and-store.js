//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1", "--useOMGJIT=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const storeTypes = [
  {type: "i32", suffix: ""},
  {type: "i64", suffix: ""},
  {type: "f32", suffix: ""},
  {type: "f64", suffix: ""},
  {type: "i32", suffix: "8"},
  {type: "i32", suffix: "16"},
  {type: "i64", suffix: "8"},
  {type: "i64", suffix: "16"},
  {type: "i64", suffix: "32"},
];

const loadTypes = [
  {type: "i32", suffix: ""},
  {type: "i64", suffix: ""},
  {type: "f32", suffix: ""},
  {type: "f64", suffix: ""},
  {type: "i32", suffix: "8_s"},
  {type: "i32", suffix: "8_u"},
  {type: "i32", suffix: "16_s"},
  {type: "i32", suffix: "16_u"},
  {type: "i64", suffix: "8_s"},
  {type: "i64", suffix: "8_u"},
  {type: "i64", suffix: "16_s"},
  {type: "i64", suffix: "16_u"},
  {type: "i64", suffix: "32_s"},
  {type: "i64", suffix: "32_u"},
];

function getWasmTypeWidth(wasmType) {
  return wasmType.endsWith("64") ? 8n : 4n;
}


let wat = `
(module
    (memory i64 1)
    ${
      storeTypes.map(storeType =>
        `(func (export "${storeType.type}_store${storeType.suffix}") (param $sz i64) (param $data ${storeType.type})
            (${storeType.type}.store${storeType.suffix} (local.get $sz) (local.get $data))
        )`).join('')
    }
    ${
      loadTypes.map(loadType =>
        `(func (export "${loadType.type}_load${loadType.suffix}") (param $sz i64) (result ${loadType.type})
            (${loadType.type}.load${loadType.suffix} (local.get $sz))
        )`).join('')
    }
)`;

const instance = await instantiate(wat, {}, {memory64: true});
const exports = instance.exports;

function test() {
    const storeAndLoad = (expectedValue) => {
      let index = 0n;  // BigInt for i64 parameter
      storeTypes.forEach((storeType) => {
        const valueToLoad = storeType.type == "i64" ? BigInt(expectedValue) : expectedValue;
        // store value 42 at index
        exports[`${storeType.type}_store${storeType.suffix}`](index, valueToLoad);
        // increment index by the width that was stored
        index += getWasmTypeWidth(storeType.type);
      });

      index = 0n;
      loadTypes.forEach((loadType) => {
        // load value from index
        const result = exports[`${loadType.type}_load${loadType.suffix}`](index);

        assert.eq(Number(result), Number(expectedValue));

        // read the same adress for signed and unsigned values
        if (!loadType.suffix.endsWith("_s"))
          // increment index by the width that was stored
          index += getWasmTypeWidth(loadType.type);
      });
    }

    storeAndLoad(42);
    // reset
    storeAndLoad(0);
}

for (let i = 0; i < wasmTestLoopCount; i++)
    test();
