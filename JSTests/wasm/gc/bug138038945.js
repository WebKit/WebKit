//@ runWebAssemblySuite("--useWasmGC=true")
import { instantiate } from "./wast-wrapper.js";

try {
    instantiate(`
    (module
      (func (export "myFunction") (param $p0 i32) (param $p1 i32) (param $p2 i32) (result i32)
          ref.null func             
          ref.cast (ref 0) (local.get 0)
          local.set $p0               
          ref.null func             
          f32.const 1.0               
          f32.const 5.0               
          f32.ne                      
          ref.null func             
          br_on_null 0                
          i32.const 0                 
      )
    )
  `);
} catch (e) {
    if (!(e instanceof WebAssembly.CompileError))
        throw new Error("bad");
}

