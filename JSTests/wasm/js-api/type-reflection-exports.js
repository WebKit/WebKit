import { compile } from "../wabt-wrapper.js";
import * as assert from "../assert.js"

function sameValue(a, b) {
    if (JSON.stringify(a) !== JSON.stringify(b))
        throw new Error(`Expected ${JSON.stringify(b)} but got ${JSON.stringify(a)}`);
}

const wat = `
(module
  (func $log1 (export "fn1") (param i64) (param i32) (result i32)
    (i32.const 0))
  (func $log2 (export "fn2") (param i32)
    (drop (local.get 0)))
  (func $log3 (export "fn3") (param i32) (param i64) (result i64)
    (local.get 1))
  (func $log4 (export "fn4") (param f32) (param f64) (result f64)
    (local.get 1))
  (func $log5 (export "fn5") (param i32) (param i32) (param i32) (result i32)
    (local.get 0))
  (func $log6 (export "fn6") (param f32) (param f32) (result f32)
    (local.get 0))
  (func $log7 (export "fn7") (param i32) (result i32)
    (local.get 0))
  (func $log8 (export "fn8") (param f64) (result f64)
    (local.get 0))

  (memory (export "memory") 1 8)

  (table (export "table_funcref") 1 2 funcref)
  (table (export "table_externref") 1 2 externref)

  (global $global_i32_mut (export "global_i32_mut") (mut i32) (i32.const 0))
  (global $global_i32_imut (export "global_i32_imut") i32 (i32.const 0))
  (global $global_f32_mut (export "global_f32_mut") (mut f32) (f32.const 0.0))
  (global $global_f64_imut (export "global_f64_imut") f64 (f64.const 0.0))
  (global $global_i64_mut (export "global_i64_mut") (mut i64) (i64.const 0))
  (global $global_i64_imut (export "global_i64_imut") i64 (i64.const 0))
  (global $global_f64_mut (export "global_f64_mut") (mut f64) (f64.const 0.0))
  (global $global_f32_imut (export "global_f32_imut") f32 (f32.const 0.0))
)
`;

async function test() {
    const module = await compile(wat);
    const expected = [
        { parameters: ["i64", "i32"], results: ["i32"] },
        { parameters: ["i32"], results: [] },
        { parameters: ["i32", "i64"], results: ["i64"] },
        { parameters: ["f32", "f64"], results: ["f64"] },
        { parameters: ["i32", "i32", "i32"], results: ["i32"] },
        { parameters: ["f32", "f32"], results: ["f32"] },
        { parameters: ["i32"], results: ["i32"] },
        { parameters: ["f64"], results: ["f64"] },
        { maximum: 8, minimum: 1, shared: false },
        { maximum: 2, minimum: 1, element: "funcref" },
        { maximum: 2, minimum: 1, element: "externref" },
        { mutable: true, value: "i32" },
        { mutable: false, value: "i32" },
        { mutable: true, value: "f32" },
        { mutable: false, value: "f64" },
        { mutable: true, value: "i64" },
        { mutable: false, value: "i64" },
        { mutable: true, value: "f64" },
        { mutable: false, value: "f32" }
    ];
    const result = WebAssembly.Module.exports(module).map(({ type }) => type);
    for (let i = 0; i < expected.length; ++i) {
        sameValue(result[i], expected[i]);
    }
}

await assert.asyncTest(test());
