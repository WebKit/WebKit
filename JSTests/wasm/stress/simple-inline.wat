;; wat2wasm --enable-exceptions simple-inline.wat -o simple-inline.wasm
(module
  (tag $e (export "tag") (param i32))
  (memory $m 1)
  (func $inlinee
    i32.const 0
    (i32.and (i32.add (i32.load (i32.const 0)) (i32.const 1)) (i32.const 0x3))
    i32.store
  )
  (func $inliner (result i32)
    (local $i i32)
    (local $sum i32)

    (loop $L0
      local.get $i
      i32.const 1
      i32.add
      local.set $i

      call $inlinee
      (i32.load (i32.const 0))
      local.get $sum
      i32.add
      local.set $sum

      local.get $i
      i32.const 50
      i32.lt_s
      br_if $L0
    )

    local.get $sum
  )
  (func $main (export "main")
    (local $i i32)
    (loop $L0
      local.get $i
      i32.const 1
      i32.add
      local.set $i

      call $inliner
      (drop)

      local.get $i
      i32.const 1000
      i32.lt_s
      br_if $L0
    )

    call $inliner
    local.set $i
    local.get $i
    i32.const 75
    i32.ne
    (if (then
      local.get $i
      throw $e
    ))
  )
)
