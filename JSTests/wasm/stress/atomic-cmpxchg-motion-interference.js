//@ requireOptions("--useWasmFaultSignalHandler=false")

import { instantiate } from "../wabt-wrapper.js";

let n = 1e5;
const f = (await instantiate(`
(module
  (import "e" "r" (func $r (param i32)))
  (memory 2)
  (func (export "f") (param i32)
    i32.const 65536
    i32.const 0
    i32.const 0
    i32.atomic.rmw.cmpxchg
    local.get 0
    memory.grow
    call $r
    if
      unreachable
    end))`, {
    e: { r() { n-- || new ArrayBuffer(0, { maxByteLength: 2 ** 17 }); } }
}, { threads: true })).exports.f;

while (n)
    f();
f(1);
