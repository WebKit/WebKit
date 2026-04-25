/*
(module
  (type $0 (func))
      (type $1 (func (result f64)))
      (func $0 (type 0))
      (func $1
              (type 1)
              (loop (result f64) (f64.const 0.0) (i32.const 0) (br_table 1) (call 0))
              (br 0)
              (unreachable)
            )
      (export "runf64" (func 1))
)
*/

let buffer = new Uint8Array([ 0,97,115,109,1,0,0,0,1,136,128,128,128,0,2,96,0,0,96,0,1,124,3,131,128,128,128,0,2,0,1,7,138,128,128,128,0,1,6,114,117,110,102,54,52,0,1,10,165,128,128,128,0,2,130,128,128,128,0,0,11,152,128,128,128,0,0,3,124,68,0,0,0,0,0,0,0,0,65,0,14,0,1,16,0,11,12,0,0,11 ]);

let m = new WebAssembly.Instance(new WebAssembly.Module(buffer));
m.exports.runf64();
