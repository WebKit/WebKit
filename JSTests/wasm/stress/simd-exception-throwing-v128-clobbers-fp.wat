;; wat2wasm --enable-all simd-exception-throwing-v128-clobbers-fp.wat -o simd-exception-throwing-v128-clobbers-fp.wasm
(module
  (type $t0 (func (param i32 i32 i32) (result i32)))
  (type $t1 (func (result f64)))
  (type $t2 (func (param i32 v128 i32 f32 v128 v128 f64 v128 v128 v128 v128 v128 f64 v128 f64)))
  (type $t3 (func (result i32 v128 v128 f64 v128 f64 v128 f64 v128)))
  (type $t4 (func (param v128 f64 f32 v128 f32 v128 v128 i64 v128 v128 f64 v128)))
  (type $t5 (func (param f64 v128 v128 f32 i64 v128)))
  (func $main (export "main") (type $t0) (param $p0 i32) (param $p1 i32) (param $p2 i32) (result i32)
    (local $l3 i32) (local $l4 i32) (local $l5 v128)
    (if $I1 (result i32)
      (i32.trunc_f32_u
        (f32.abs
          (f32.nearest
            (f32.nearest
              (f32.abs
                (f32.nearest
                  (f32.convert_i32_s
                    (call_indirect $T0 (type $t0)
                      (i32.shr_u
                        (try (result i32)  ;; label = @2
                          (do
                            (f32.demote_f64
                              (call $f1))
                            (call $f3)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (f32.convert_i32_s)
                            (local.set $p0
                              (i32.const 0))
                            (call $f3)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (f32.convert_i32_s)
                            (f32.add)
                            (i32.store align=1
                              (i32.const 0)
                              (i32.const 0))
                            (i32.trunc_f32_u
                              (f32.sqrt
                                (f32.mul
                                  (f32.abs
                                    (f32.trunc
                                      (f32.abs
                                        (f32.nearest
                                          (f32.nearest
                                            (f32.abs
                                              (f32.sqrt
                                                (f32.nearest
                                                  (f32.trunc
                                                    (f32.abs
                                                      (f32.nearest
                                                        (f32.abs
                                                          (f32.nearest
                                                            (f32.const 0x1.91bd8cp-127 (;=9.22351e-39;)))))))))))))))
                                  (f32.add)))))
                          (catch $e0
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (i32.const 0))
                          (catch $e1
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (drop)
                            (i32.trunc_f64_s))
                          (catch_all
                            (i32.const 0)))
                        (i8x16.extract_lane_u 0
                          (i8x16.max_u
                            (i8x16.splat
                              (i32.const 544695662))
                            (i32x4.shr_u
                              (i64x2.extmul_low_i32x4_u
                                (i16x8.shl
                                  (i8x16.splat
                                    (i32.const 0))
                                  (i32.trunc_f32_u
                                    (f32.abs
                                      (f32.const 0x1.586062p-29 (;=2.50567e-09;)))))
                                (i8x16.splat
                                  (i32.const 51)))
                              (block $B0 (result i32)
                                (i32.trunc_f32_u
                                  (f32.sqrt
                                    (f32.copysign
                                      (f32.const 0x1.647064p-15 (;=4.24908e-05;))
                                      (f32.const 0x1.586058p-27 (;=1.00227e-08;))))))))))
                      (i32.const 0)
                      (i32.const 0)
                      (i32.const 0)))))))))
      (then
        (i32.const 0))
      (else
        (i32.const 0))))
  (func $f1 (type $t1) (result f64)
    (local $l0 f32) (local $l1 v128) (local $l2 v128) (local $l3 v128) (local $l4 v128) (local $l5 v128) (local $l6 v128) (local $l7 v128) (local $l8 v128) (local $l9 i64) (local $l10 v128) (local $l11 v128) (local $l12 i32) (local $l13 f64) (local $l14 v128) (local $l15 v128) (local $l16 f64) (local $l17 v128)
    (f64.trunc
      (f64.abs
        (f64.div
          (f64.const 0x1.b9918a1p-1045 (;=4.57523e-315;))
          (f64.abs
            (f64.nearest
              (f64.abs
                (f64.nearest
                  (f64.sqrt
                    (f64.sqrt
                      (f64.abs
                        (f64.copysign
                          (f64.const 0x1.6188p-1061 (;=5.58936e-320;))
                          (f64.nearest
                            (f64.sqrt
                              (f64.const 0x1.c36552c302837p-237 (;=7.98378e-72;)))))))))))))))
    (f64.const 0x0p+0 (;=0;))
    (f64.abs
      (f64.trunc
        (f64.abs
          (f64.nearest
            (f64.abs
              (f64.div
                (f64.const 0x1.61b9896188p-1037 (;=9.38253e-313;))
                (f64.abs
                  (f64.trunc
                    (f64.abs
                      (f64.nearest
                        (f64.abs
                          (f64.div
                            (f64.trunc
                              (f64.abs
                                (f64.div
                                  (f64.const 0x1.b99189618p-1037 (;=1.17126e-312;))
                                  (f64.const 0x1.91896p-1053 (;=1.62517e-317;)))))
                            (f64.const 0x1.61896164p-1037 (;=9.37754e-313;))))))))))))))
    (throw $e0
      (i16x8.max_u
        (i16x8.ge_s
          (i16x8.shr_s
            (f64x2.ceil
              (i16x8.add
                (i16x8.shr_u
                  (i16x8.sub
                    (i8x16.splat
                      (i32.const 49))
                    (i8x16.splat
                      (i32.const 2896697)))
                  (i32.trunc_f32_s
                    (f32.abs
                      (f32.div
                        (f32.const 0x0p+0 (;=0;))
                        (f32.const 0x1.61616p-128 (;=4.05661e-39;))))))
                (i16x8.shr_s
                  (i8x16.splat
                    (i32.const 0))
                  (i32.reinterpret_f32
                    (f32.sqrt
                      (f32.abs
                        (f32.nearest
                          (f32.sub
                            (f32.const 0x1.88p-144 (;=6.86636e-44;))
                            (f32.const 0x1.88p-144 (;=6.86636e-44;))))))))))
            (i32.const 44))
          (i8x16.splat
            (i32.const 4994358)))
        (i16x8.shl
          (i16x8.shr_s
            (i8x16.splat
              (i32.const 59))
            (i32.lt_s
              (i32.const 0)
              (i32.const 0)))
          (i32.trunc_f64_u
            (f64.const 0x1.8998p-1061 (;=6.22276e-320;)))))
      (f64.const 0x0p+0 (;=0;))
      (f32.const 0x0p+0 (;=0;))
      (i8x16.splat
        (i32.const 0))
      (f32.const 0x1.a9b16p-128 (;=4.88671e-39;))
      (i8x16.splat
        (i32.const 0))
      (i8x16.splat
        (i32.const 0))
      (i64.const 0)
      (i8x16.splat
        (i32.const 0))
      (i8x16.splat
        (i32.const 0))
      (f64.const 0x0p+0 (;=0;))
      (i16x8.ge_s
        (i16x8.shr_s
          (i64x2.shl
            (i8x16.splat
              (i32.const 0))
            (select (result i32)
              (i32.const 0)
              (i32.const 0)
              (i32.const 0)))
          (i32.clz
            (i8x16.extract_lane_u 0
              (i8x16.max_u
                (i8x16.splat
                  (i32.const 9921902))
                (i8x16.max_u
                  (i8x16.splat
                    (i32.const 3487074))
                  (i8x16.splat
                    (i32.const 53)))))))
        (i16x8.add_sat_u
          (i16x8.add_sat_u
            (i8x16.splat
              (i32.const 53))
            (i8x16.splat
              (i32.const 53)))
          (i8x16.splat
            (i32.const 7107940)))))
    (f64.abs
      (f64.trunc
        (f64.abs
          (f64.nearest
            (f64.trunc
              (f64.abs
                (f64.nearest
                  (f64.abs
                    (f64.sub)))))))))
    (f64.copysign))
  (func $f2 (type $t2) (param $p0 i32) (param $p1 v128) (param $p2 i32) (param $p3 f32) (param $p4 v128) (param $p5 v128) (param $p6 f64) (param $p7 v128) (param $p8 v128) (param $p9 v128) (param $p10 v128) (param $p11 v128) (param $p12 f64) (param $p13 v128) (param $p14 f64)
    (local $l15 f32) (local $l16 v128) (local $l17 v128) (local $l18 f64) (local $l19 i64) (local $l20 v128) (local $l21 i32) (local $l22 i64) (local $l23 i64) (local $l24 i64) (local $l25 v128) (local $l26 i64) (local $l27 v128) (local $l28 v128) (local $l29 v128) (local $l30 i32) (local $l31 i64) (local $l32 i32) (local $l33 f64) (local $l34 f32) (local $l35 v128) (local $l36 i32) (local $l37 i32)
    (if $I0
      (global.get $g47)
      (then
        (i64.atomic.store offset=1668178292
          (i32.const 63)
          (i64.const 32)))))
  (func $f3 (type $t3) (result i32 v128 v128 f64 v128 f64 v128 f64 v128)
    (local $l0 f32) (local $l1 f32) (local $l2 i64) (local $l3 f64) (local $l4 i32) (local $l5 i32) (local $l6 i32) (local $l7 i64) (local $l8 v128) (local $l9 f64) (local $l10 v128) (local $l11 i64) (local $l12 i64) (local $l13 f64) (local $l14 v128) (local $l15 i32) (local $l16 f64) (local $l17 i64) (local $l18 i64) (local $l19 i64) (local $l20 v128) (local $l21 v128) (local $l22 v128)
    (i32.const 0)
    (i8x16.splat
      (i32.const 0))
    (i8x16.splat
      (i32.const 0))
    (f64.const 0x0p+0 (;=0;))
    (i8x16.splat
      (i32.const 0))
    (f64.const 0x0p+0 (;=0;))
    (i8x16.splat
      (i32.const 0))
    (f64.const 0x0p+0 (;=0;))
    (i8x16.splat
      (i32.const 0)))
  (table $T0 4 11 funcref)
  (table $T1 3 9 funcref)
  (table $T2 3 9 funcref)
  (table $T3 3 9 funcref)
  (memory $M0 16 32)
  (tag $e0 (type $t4) (param v128 f64 f32 v128 f32 v128 v128 i64 v128 v128 f64 v128))
  (tag $e1 (type $t5) (param f64 v128 v128 f32 i64 v128))
  (global $g0 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g1 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g2 (mut i64) (i64.const 0))
  (global $g3 (mut f64) (f64.const 0x0p+0 (;=0;)))
  (global $g4 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g5 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g6 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g7 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g8 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g9 (mut i32) (i32.const 0))
  (global $g10 (mut f32) (f32.const 0x0p+0 (;=0;)))
  (global $g11 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g12 (mut i32) (i32.const 0))
  (global $g13 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g14 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g15 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g16 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g17 (mut i32) (i32.const 0))
  (global $g18 (mut f32) (f32.const 0x0p+0 (;=0;)))
  (global $g19 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g20 (mut i32) (i32.const 0))
  (global $g21 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g22 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g23 (mut f64) (f64.const 0x0p+0 (;=0;)))
  (global $g24 (mut i64) (i64.const 0))
  (global $g25 (mut i64) (i64.const 0))
  (global $g26 (mut i64) (i64.const 0))
  (global $g27 (mut i64) (i64.const 0))
  (global $g28 (mut i64) (i64.const 0))
  (global $g29 (mut i64) (i64.const 0))
  (global $g30 i64 (i64.const 0))
  (global $g31 (mut i64) (i64.const 0))
  (global $g32 (mut i32) (i32.const 0))
  (global $g33 (mut i32) (i32.const 0))
  (global $g34 (mut i64) (i64.const 0))
  (global $g35 (mut i64) (i64.const 0))
  (global $g36 (mut f32) (f32.const 0x0p+0 (;=0;)))
  (global $g37 (mut f32) (f32.const 0x0p+0 (;=0;)))
  (global $g38 (mut f64) (f64.const 0x0p+0 (;=0;)))
  (global $g39 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g40 (mut i32) (i32.const 0))
  (global $g41 (mut f32) (f32.const 0x0p+0 (;=0;)))
  (global $g42 (mut i32) (i32.const 0))
  (global $g43 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g44 (mut i32) (i32.const 0))
  (global $g45 (mut v128) (v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000))
  (global $g46 (mut i32) (i32.const 0))
  (global $g47 (mut i32) (i32.const 0))
  (elem $e0 (i32.const 0) func $main $f1 $f2 $f3)
  (elem $e1 (table $T1) (i32.const 0) func $main $f1 $f2)
  (elem $e2 (table $T2) (i32.const 0) func $main $f1 $f2)
  (elem $e3 (table $T3) (i32.const 0) func $main $f1 $f2))
