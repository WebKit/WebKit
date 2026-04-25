// (module
//  (rec
//    (type (;0;) (struct (field (mut i32))))
//    (type (;1;) (struct (field (mut i32)) (field (mut i64)) (field (mut i64))))
//  )
//  (type (;2;) (func (result (ref struct))))
//  (type (;3;) (func (result i32)))
//  (export "fail" (func 0))
//  (export "pass" (func 1))
//  (func (;0;) (type 3) (result i32)
//    block (type 2) (result (ref struct)) ;; label = @1
//      i32.const 42
//      struct.new 0
//      br_on_cast_fail 0 (;@1;) (ref struct) (ref 1)
//      drop
//      i32.const 0
//      return
//    end
//    drop
//    i32.const 1
//  )
//  (func (;1;) (type 3) (result i32)
//    block (type 2) (result (ref struct)) ;; label = @1
//      i32.const 99
//      i64.const 0
//      i64.const 0
//      struct.new 1
//      br_on_cast_fail 0 (;@1;) (ref struct) (ref 1)
//      drop
//      i32.const 1
//      return
//    end
//    drop
//    i32.const 0
//  )
// )

const bytes = new Uint8Array([0,97,115,109,1,0,0,0,1,28,3,78,2,79,0,95,1,127,1,79,0,95,3,127,1,126,1,126,1,96,0,1,100,107,96,0,1,127,3,3,2,3,3,7,15,2,4,102,97,105,108,0,0,4,112,97,115,115,0,1,10,54,2,23,0,2,2,65,42,251,0,0,251,25,0,0,107,1,26,65,0,15,11,26,65,1,11,28,0,2,2,65,227,0,66,0,66,0,251,0,1,251,25,0,0,107,1,26,65,1,15,11,26,65,0,11]);
const mod = new WebAssembly.Module(bytes);
const { pass, fail } = new WebAssembly.Instance(mod).exports;

for (let i = 0; i < 200000; i++) {
    fail();
    pass();
}

if (fail() === 0 || pass() === 0)
    throw new Error("br_cast_on_fail incorrect");
