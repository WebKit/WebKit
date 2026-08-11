    (local $i i32) (local $l24 v128) (local $l41 v128) 
    (local.set $i
      (i32.const 2))
    (local.set $l41
      (v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF))

    (block $B0
      (loop $L1

        (block $B2
          (call $probe_begin (i32.const -1))
          (call $probe (i64x2.extract_lane 0 (local.get $l41)))
          (call $probe (i64x2.extract_lane 1 (local.get $l41)))
          (call $probe (i64x2.extract_lane 0 (local.get $l24)))
          (call $probe (i64x2.extract_lane 1 (local.get $l24)))
          (call $probe_end (i32.const -1))

          (br_if $B2
            (i32.eqz
              (v128.any_true
                (local.get $l41))))
          (local.set $l24
            (v128.not
              (local.get $l41)))
         )

        (br_if $B0
          (i32.eqz
            (local.tee $i
              (i32.sub
                (local.get $i)
                (i32.const 1)))))
        (br $L1))))
