//@ skip if !$isSIMDPlatform

try {

    /*

    (module
      (type $0 (func))
      (type $1 (func (param i32) (result i32)))
      (tag $e (param v128))
      (export "simple_throw_catch_v128" (func 0))
      (func $0
        (type 1)
        (block $b (result v128)
          (try_table
            (result v128)
            (catch $e 75)
            (local.get 0)
            (i32.eqz)
            (if (then (throw $e (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16))) (else))
            (v128.const i8x16 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42)
          )
          (i8x16.extract_lane_u 12)
          (return)
        )
        (i8x16.extract_lane_u 12)
      )
    )

    */
    new WebAssembly.Module(new Uint8Array([
        0,97,115,109,1,0,0,0,1,141,128,128,128,0,3,96,0,0,96,1,127,1,127,96,1,123,0,3,130,128,128,128,0,1,1,13,131,128,128,128,0,1,0,2,7,155,128,128,128,0,1,23,115,105,109,112,108,101,95,116,104,114,111,119,95,99,97,116,99,104,95,118,49,50,56,0,0,10,197,128,128,128,0,1,191,128,128,128,0,0,2,123,31,123,1,0,0,75,32,0,69,4,64,253,12,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,8,0,11,253,12,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,11,253,22,12,15,11,253,22,12,11
    ]));
} catch (e) {
    if (!e || !e.toString().startsWith("CompileError: WebAssembly.Module doesn't parse at byte 9: try_table's catch target 75 exceeds control stack size 2, in function at index 0"))
        throw e;
}


// This is an old version of this test that failed but had other, earlier validation issues so it now no longer errors with the original issue.
try {
    new WebAssembly.Module(new Uint8Array([
        , 97, 115, 109, 1, , , , 1, 7, 1, 96, 3, 127, 127, 127, , 2, 12, 1, 2, , ,
        3, , , , 2, 1, , , 3, 2, 1, , , 1, , , 13, , , , , , , , , , , , , , 10,
        57, 1, 55, 1, 1, 127, 65, , 33, , 3, 64, 2, 64, 32, , 32, , 70, -61139, 0,
        , 1, 65, 4, 108, 32, 3, 65, 0, 108, 106, 31, 0, 32, 3, , 4, 1, 1, 40, 0, 0,
        , 0, 0, 32, 3, 65, 1, 1, 33, 7, , 1, 11, , 1
    ]));
} catch (e) {
   if (!e || !e.toString().startsWith("CompileError: WebAssembly.Module doesn't parse at byte 35: invalid opcode of try_table catch at index 1,  opcode 4 is invalid, in function at index 0"))
       throw e;
}

