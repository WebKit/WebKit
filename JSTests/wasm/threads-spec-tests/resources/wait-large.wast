(module
  (memory 8192 8192 shared)

  (func (export "init") (param $value i64) (i64.store (i32.const 134217720) (local.get $value)))

  (func (export "memory.atomic.notify") (param $addr i32) (param $count i32) (result i32)
      (memory.atomic.notify (local.get 0) (local.get 1)))
  (func (export "memory.atomic.wait32") (param $addr i32) (param $expected i32) (param $timeout i64) (result i32)
      (memory.atomic.wait32 (local.get 0) (local.get 1) (local.get 2)))
  (func (export "memory.atomic.wait64") (param $addr i32) (param $expected i64) (param $timeout i64) (result i32)
      (memory.atomic.wait64 (local.get 0) (local.get 1) (local.get 2)))
)

(invoke "init" (i64.const 0xffffffffffff))
(assert_return (invoke "memory.atomic.wait32" (i32.const 134217720) (i32.const 0) (i64.const 0)) (i32.const 1))
(assert_return (invoke "memory.atomic.wait64" (i32.const 134217720) (i64.const 0) (i64.const 0)) (i32.const 1))
(assert_return (invoke "memory.atomic.notify" (i32.const 134217720) (i32.const 0)) (i32.const 0))
