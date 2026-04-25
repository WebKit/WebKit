//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform

// (module
//   (type $0 (array (mut i8)))
//   (type $1 (func))
//   (type $2 (func (param (ref $0) (ref struct) (ref array))))
//   (type $3 (func (param (ref eq) (ref eq) (ref eq))))
//   (type $4 (func (param i32 i32 i32) (result i32)))
//   (type $5 (array (mut i16)))
//   (import "mod" "foo0" (func $foo0))
//   (import "mod" "foo1" (func $foo1))
//   (memory $0 0 32)
//   (table $0 1 1 funcref)
//   (elem $0 (i32.const 0) $0)
//   (tag $tag$0 (type $2) (param (ref $0) (ref struct) (ref array)))
//   (tag $tag$1 (type $3) (param (ref eq) (ref eq) (ref eq)))
//   (export "main" (func $0))
//   (func $0 (param $0 i32) (param $1 i32) (param $2 i32) (result i32)
//    (local $3 (ref null $5))
//    (local $scratch (tuple (ref $0) (ref struct) (ref array)))
//    (local $scratch_5 (ref struct))
//    (local $scratch_6 (ref $0))
//    (local $scratch_7 (tuple (ref eq) (ref eq) (ref eq)))
//    (local $scratch_8 (ref eq))
//    (local $scratch_9 (ref eq))
//    (local $10 (tuple (ref $0) (ref struct) (ref array)))
//    (local $11 (tuple (ref eq) (ref eq) (ref eq)))
//    (try (result i32)
//     (do
//      (try (result i32)
//       (do
//        (i32.lt_u
//         (i32.lt_s
//          (i32.lt_s
//           (i32.const 251710)
//           (i32.const 4053464)
//          )
//          (i32.lt_s
//           (i32x4.bitmask
//            (i8x16.splat
//             (i32.const 0)
//            )
//           )
//           (i32x4.bitmask
//            (i8x16.splat
//             (i32.const 0)
//            )
//           )
//          )
//         )
//         (i32x4.bitmask
//          (f32x4.abs
//           (f32x4.abs
//            (f32x4.abs
//             (f32x4.abs
//              (f32x4.abs
//               (f32x4.abs
//                (f32x4.abs
//                 (f32x4.abs
//                  (f32x4.abs
//                   (f32x4.abs
//                    (f32x4.abs
//                     (f32x4.abs
//                      (f32x4.abs
//                       (i64x2.splat
//                        (i64.or
//                         (i64.const 4927590540962014341)
//                         (i64.shr_s
//                          (i64.shr_s
//                           (i64.const -2967618813368517134)
//                           (i64.shr_s
//                            (i64.shr_s
//                             (i64.const -1481889706834512670)
//                             (i64.const 2260158641624739814)
//                            )
//                            (i64.shr_s
//                             (i64.const 4119973288211384747)
//                             (i64.const 7340594061637065941)
//                            )
//                           )
//                          )
//                          (i64.shr_s
//                           (i64.shr_s
//                            (i64.shr_s
//                             (i64.shr_s
//                              (i64.shr_s
//                               (i64.const -1913269819194553401)
//                               (i64.const -6322282129093605079)
//                              )
//                              (i64.const 1044766)
//                             )
//                             (i64.shr_s
//                              (i64.shr_s
//                               (i64.shr_s
//                                (i64.const 3499130373663735137)
//                                (i64.const -8879168641383576964)
//                               )
//                               (i64.shr_s
//                                (i64.const 3475801533409820633)
//                                (i64.const -4386333858421910755)
//                               )
//                              )
//                              (i64.shr_s
//                               (i64.shr_s
//                                (i64.const 960038910559385339)
//                                (i64.const -6730800373085259387)
//                               )
//                               (i64.shr_s
//                                (i64.const -1570915167053168056)
//                                (i64.const 5822220106237417307)
//                               )
//                              )
//                             )
//                            )
//                            (i64.shr_s
//                             (i64.const 3247601926281549413)
//                             (i64.shr_s
//                              (i64.const 650871402118812180)
//                              (i64.const 6624749069071103411)
//                             )
//                            )
//                           )
//                           (i64.shr_s
//                            (i64.shl
//                             (i64.const -7694691238001865310)
//                             (i64.popcnt
//                              (i64.const -4889171535231970060)
//                             )
//                            )
//                            (i64.const 8908524914694640535)
//                           )
//                          )
//                         )
//                        )
//                       )
//                      )
//                     )
//                    )
//                   )
//                  )
//                 )
//                )
//               )
//              )
//             )
//            )
//           )
//          )
//         )
//        )
//       )
//       (catch $tag$0
//        (local.set $10
//         (pop (tuple (ref $0) (ref struct) (ref array)))
//        )
//        (block (result i32)
//         (drop
//          (block (result (ref $0))
//           (local.set $scratch_6
//            (tuple.extract 3 0
//             (local.tee $scratch
//              (local.get $10)
//             )
//            )
//           )
//           (drop
//            (block (result (ref struct))
//             (local.set $scratch_5
//              (tuple.extract 3 1
//               (local.get $scratch)
//              )
//             )
//             (drop
//              (tuple.extract 3 2
//               (local.get $scratch)
//              )
//             )
//             (local.get $scratch_5)
//            )
//           )
//           (local.get $scratch_6)
//          )
//         )
//         (i32.const 0)
//        )
//       )
//       (catch $tag$1
//        (local.set $11
//         (pop (tuple (ref eq) (ref eq) (ref eq)))
//        )
//        (block (result i32)
//         (drop
//          (block (result (ref eq))
//           (local.set $scratch_9
//            (tuple.extract 3 0
//             (local.tee $scratch_7
//              (local.get $11)
//             )
//            )
//           )
//           (drop
//            (block (result (ref eq))
//             (local.set $scratch_8
//              (tuple.extract 3 1
//               (local.get $scratch_7)
//              )
//             )
//             (drop
//              (tuple.extract 3 2
//               (local.get $scratch_7)
//              )
//             )
//             (local.get $scratch_8)
//            )
//           )
//           (local.get $scratch_9)
//          )
//         )
//         (i32.const 2712)
//        )
//       )
//      )
//     )
//     (catch_all
//      (i32.const 29897)
//     )
//    )
//   )
var wasm_code = new Uint8Array([0,97,115,109,1,0,0,0,1,175,128,128,128,0,7,78,1,94,120,1,78,1,94,119,1,78,1,80,0,95,0,78,1,96,3,127,127,127,1,127,96,3,100,0,100,107,100,106,0,96,3,100,109,100,109,100,109,0,96,0,0,2,151,128,128,128,0,2,3,109,111,100,4,102,111,111,48,0,6,3,109,111,100,4,102,111,111,49,0,6,3,130,128,128,128,0,1,3,4,133,128,128,128,0,1,112,1,1,1,5,132,128,128,128,0,1,1,0,32,13,133,128,128,128,0,2,0,4,0,5,7,136,128,128,128,0,1,4,109,97,105,110,0,2,9,139,128,128,128,0,1,6,0,65,0,11,112,1,210,2,11,10,234,130,128,128,0,1,231,2,1,1,99,1,6,127,6,127,65,190,174,15,65,216,179,247,1,72,65,0,253,15,253,164,1,65,0,253,15,253,164,1,72,72,66,133,201,246,132,213,175,148,177,196,0,66,242,251,156,131,153,205,185,232,86,66,226,233,200,222,155,165,209,183,107,66,230,223,201,208,244,201,236,174,31,135,66,171,147,192,191,136,188,197,150,57,66,213,185,224,225,130,237,193,239,229,0,135,135,135,66,199,151,187,155,200,231,172,185,101,66,169,146,205,172,232,188,175,161,168,127,135,66,158,226,63,135,66,225,250,134,136,146,145,218,199,48,66,252,148,173,236,161,229,182,227,132,127,135,66,217,255,153,229,195,226,161,158,48,66,157,166,249,179,135,147,167,144,67,135,135,66,251,237,199,211,215,201,175,169,13,66,133,243,190,161,141,151,217,203,162,127,135,66,200,132,151,188,168,159,191,153,106,66,219,174,172,163,237,202,171,230,208,0,135,135,135,135,66,229,172,139,159,237,151,243,136,45,66,148,164,130,238,247,130,151,132,9,66,179,235,212,224,220,197,245,247,219,0,135,135,135,66,162,155,218,180,229,226,189,155,149,127,66,244,169,176,164,200,140,139,147,188,127,123,134,66,151,167,221,168,222,134,220,208,251,0,135,135,135,132,253,18,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,224,1,253,164,1,73,7,0,26,26,26,65,0,7,1,26,26,26,65,152,21,11,25,65,201,233,1,11,11,0,152,128,128,128,0,4,110,97,109,101,1,141,128,128,128,0,2,0,4,102,111,111,48,1,4,102,111,111,49]);
var wasm_module = new WebAssembly.Module(wasm_code);
function foo(){};
const o15 = {"foo0": foo,"foo1": foo};
const o16 = {"mod": o15};
var wasm_instance = new WebAssembly.Instance(wasm_module, o16);
var f = wasm_instance.exports.main;
var result1 = f(0, -0, 1);
var result2 = f(0, -0, 1);
for (var i = 0; i < 10000; i++) {
  f(-0, 0, 1);
}
var result3 = f(0, -0, 1);