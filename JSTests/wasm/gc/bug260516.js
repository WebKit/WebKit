//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import { compile, instantiate } from "./wast-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

// This disassembly is not complete as some unused types were elided by wasm-dis.
/*
 * (module
 *  (type $0 (sub (func (param i32 i32 i32) (result i32))))
 *  (type $1 (func))
 *  (memory $0 16 32)
 *  (table $0 1 1 funcref)
 *  (elem $0 (i32.const 0) $0)
 *  (tag $tag$0)
 *  (export "" (func $0))
 *  (func $0 (param $0 i32) (param $1 i32) (param $2 i32) (result i32)
 *   (drop
 *    (call $0
 *     (i32.const 697894484)
 *     (i32.const -157833023)
 *     (i32.const -434794502)
 *    )
 *   )
 *   (drop
 *    (call $0
 *     (i32.const 769991768)
 *     (i32.const 631785841)
 *     (i32.const 1294656724)
 *    )
 *   )
 *   (drop
 *    (loop $label$1 (result (ref null $0))
 *     (ref.cast (ref null $0)
 *      (table.get $0
 *       (block $label$2 (result i32)
 *        (select
 *         (i32.const 487924244)
 *         (i32.const -306468332)
 *         (i32.const -69)
 *        )
 *       )
 *      )
 *     )
 *    )
 *   )
 *   (drop
 *    (block $label$3 (type 8)
 *     (ref.func $0)
 *    )
 *   )
 *   (block $label$4 (type 9)
 *    (i32.const 1855060810)
 *   )
 *  )
 * )
 */
new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\xb6\x80\x80\x80\x00\x0a\x50\x00\x5f\x00\x50\x00\x5f\x00\x50\x00\x5f\x00\x50\x00\x5f\x00\x50\x00\x5e\x7f\x01\x50\x00\x5e\x7f\x01\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x60\x00\x00\x50\x00\x60\x01\x63\x06\x01\x70\x50\x00\x60\x01\x70\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x06\x04\x85\x80\x80\x80\x00\x01\x70\x01\x01\x01\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x83\x80\x80\x80\x00\x01\x00\x07\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x8b\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x01\xd2\x00\x0b\x0a\xda\x80\x80\x80\x00\x01\x58\x00\x41\xd4\x8c\xe4\xcc\x02\x41\xc1\xd1\xde\xb4\x7f\x41\xfa\x9f\xd6\xb0\x7e\x10\x00\x1a\x41\xd8\xc8\x94\xef\x02\x41\xf1\x92\xa1\xad\x02\x41\xd4\xc9\xab\xe9\x04\x10\x00\x1a\x03\x63\x06\x02\x7f\x41\x94\xc4\xd4\xe8\x01\x41\x94\xd4\xee\xed\x7e\x41\xbb\x7f\x1b\x0b\x25\x00\xfb\x17\x06\x0b\x02\x08\x1a\xd2\x00\x0b\x02\x09\x1a\x41\xca\xf6\xc7\xf4\x06\x0b\x0b"));
