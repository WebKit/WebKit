import { compile } from "../wabt-wrapper.js";
import * as assert from "../assert.js"

function sameValue(a, b) {
    if (JSON.stringify(a) !== JSON.stringify(b))
        throw new Error(`Expected ${JSON.stringify(b)} but got ${JSON.stringify(a)}`);
}

const wat = `
(module
  (import "mod" "fn1" (func $log1 (param i64) (param i32) (result i32)))
  (import "mod" "fn2" (func $log2 (param i32)))
  (import "mod" "fn3" (func $log3 (param i32) (param i64) (result i64)))
  (import "mod" "fn4" (func $log4 (param f32) (param f64) (result f64)))
  (import "mod" "fn5" (func $log5 (param i32) (param i32) (param i32) (result i32)))
  (import "mod" "fn6" (func $log6 (param f32) (param f32) (result f32)))
  (import "mod" "fn7" (func $log7 (param i32) (result i32)))
  (import "mod" "fn8" (func $log8 (param f64) (result f64)))

  (import "mod" "memory" (memory 1 8))

  (import "mod" "table_funcref" (table 1 2 funcref))
  (import "mod" "table_externref" (table 1 2 externref))

  (import "mod" "global_i32_mut" (global $global_i32_mut (mut i32)))
  (import "mod" "global_i32_imut" (global $global_i32_imut i32))
  (import "mod" "global_f32_mut" (global $global_f32_mut (mut f32)))
  (import "mod" "global_f64_imut" (global $global_f64_imut f64))
  (import "mod" "global_i64_mut" (global $global_i64_mut (mut i64)))
  (import "mod" "global_i64_imut" (global $global_i64_imut i64))
  (import "mod" "global_f64_mut" (global $global_f64_mut (mut f64)))
  (import "mod" "global_f32_imut" (global $global_f32_imut f32))
)
`
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
    const result = WebAssembly.Module.imports(module).map(({ type }) => type);
    for (let i = 0; i < expected.length; ++i) {
        sameValue(result[i], expected[i]);
    }
}

await assert.asyncTest(test());
