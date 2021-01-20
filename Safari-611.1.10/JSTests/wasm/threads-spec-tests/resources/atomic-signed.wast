;; atomic operations more for JavaScriptCore

(module
  (memory 1 1 shared)

  (func (export "init") (param $value i64) (i64.store (i32.const 0) (local.get $value)))

  (func (export "i32.atomic.load") (param $addr i32) (result i32) (i32.atomic.load (local.get $addr)))
  (func (export "i64.atomic.load") (param $addr i32) (result i64) (i64.atomic.load (local.get $addr)))
  (func (export "i32.atomic.load8_u") (param $addr i32) (result i32) (i32.atomic.load8_u (local.get $addr)))
  (func (export "i32.atomic.load16_u") (param $addr i32) (result i32) (i32.atomic.load16_u (local.get $addr)))
  (func (export "i64.atomic.load8_u") (param $addr i32) (result i64) (i64.atomic.load8_u (local.get $addr)))
  (func (export "i64.atomic.load16_u") (param $addr i32) (result i64) (i64.atomic.load16_u (local.get $addr)))
  (func (export "i64.atomic.load32_u") (param $addr i32) (result i64) (i64.atomic.load32_u (local.get $addr)))

  (func (export "i32.atomic.store") (param $addr i32) (param $value i32) (i32.atomic.store (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.store") (param $addr i32) (param $value i64) (i64.atomic.store (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.store8") (param $addr i32) (param $value i32) (i32.atomic.store8 (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.store16") (param $addr i32) (param $value i32) (i32.atomic.store16 (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.store8") (param $addr i32) (param $value i64) (i64.atomic.store8 (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.store16") (param $addr i32) (param $value i64) (i64.atomic.store16 (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.store32") (param $addr i32) (param $value i64) (i64.atomic.store32 (local.get $addr) (local.get $value)))

  (func (export "i32.atomic.rmw.add") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.add (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw.add") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw.add (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw8.add_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw8.add_u (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw16.add_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw16.add_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw8.add_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw8.add_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw16.add_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw16.add_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw32.add_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw32.add_u (local.get $addr) (local.get $value)))

  (func (export "i32.atomic.rmw.sub") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.sub (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw.sub") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw.sub (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw8.sub_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw8.sub_u (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw16.sub_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw16.sub_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw8.sub_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw8.sub_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw16.sub_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw16.sub_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw32.sub_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw32.sub_u (local.get $addr) (local.get $value)))

  (func (export "i32.atomic.rmw.and") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.and (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw.and") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw.and (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw8.and_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw8.and_u (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw16.and_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw16.and_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw8.and_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw8.and_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw16.and_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw16.and_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw32.and_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw32.and_u (local.get $addr) (local.get $value)))

  (func (export "i32.atomic.rmw.or") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.or (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw.or") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw.or (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw8.or_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw8.or_u (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw16.or_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw16.or_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw8.or_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw8.or_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw16.or_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw16.or_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw32.or_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw32.or_u (local.get $addr) (local.get $value)))

  (func (export "i32.atomic.rmw.xor") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.xor (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw.xor") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw.xor (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw8.xor_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw8.xor_u (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw16.xor_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw16.xor_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw8.xor_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw8.xor_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw16.xor_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw16.xor_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw32.xor_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw32.xor_u (local.get $addr) (local.get $value)))

  (func (export "i32.atomic.rmw.xchg") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.xchg (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw.xchg") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw.xchg (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw8.xchg_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw8.xchg_u (local.get $addr) (local.get $value)))
  (func (export "i32.atomic.rmw16.xchg_u") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw16.xchg_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw8.xchg_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw8.xchg_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw16.xchg_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw16.xchg_u (local.get $addr) (local.get $value)))
  (func (export "i64.atomic.rmw32.xchg_u") (param $addr i32) (param $value i64) (result i64) (i64.atomic.rmw32.xchg_u (local.get $addr) (local.get $value)))

  (func (export "i32.atomic.rmw.cmpxchg") (param $addr i32) (param $expected i32) (param $value i32) (result i32) (i32.atomic.rmw.cmpxchg (local.get $addr) (local.get $expected) (local.get $value)))
  (func (export "i64.atomic.rmw.cmpxchg") (param $addr i32) (param $expected i64)  (param $value i64) (result i64) (i64.atomic.rmw.cmpxchg (local.get $addr) (local.get $expected) (local.get $value)))
  (func (export "i32.atomic.rmw8.cmpxchg_u") (param $addr i32) (param $expected i32)  (param $value i32) (result i32) (i32.atomic.rmw8.cmpxchg_u (local.get $addr) (local.get $expected) (local.get $value)))
  (func (export "i32.atomic.rmw16.cmpxchg_u") (param $addr i32) (param $expected i32)  (param $value i32) (result i32) (i32.atomic.rmw16.cmpxchg_u (local.get $addr) (local.get $expected) (local.get $value)))
  (func (export "i64.atomic.rmw8.cmpxchg_u") (param $addr i32) (param $expected i64)  (param $value i64) (result i64) (i64.atomic.rmw8.cmpxchg_u (local.get $addr) (local.get $expected) (local.get $value)))
  (func (export "i64.atomic.rmw16.cmpxchg_u") (param $addr i32) (param $expected i64)  (param $value i64) (result i64) (i64.atomic.rmw16.cmpxchg_u (local.get $addr) (local.get $expected) (local.get $value)))
  (func (export "i64.atomic.rmw32.cmpxchg_u") (param $addr i32) (param $expected i64)  (param $value i64) (result i64) (i64.atomic.rmw32.cmpxchg_u (local.get $addr) (local.get $expected) (local.get $value)))

)

;; *.atomic.load*

(invoke "init" (i64.const 0xffffffffffffffff))

(assert_return (invoke "i32.atomic.load" (i32.const 0)) (i32.const 0xffffffff))
(assert_return (invoke "i32.atomic.load" (i32.const 4)) (i32.const 0xffffffff))

(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffffffffffff))

(assert_return (invoke "i32.atomic.load8_u" (i32.const 0)) (i32.const 0xff))
(assert_return (invoke "i32.atomic.load8_u" (i32.const 5)) (i32.const 0xff))

(assert_return (invoke "i32.atomic.load16_u" (i32.const 0)) (i32.const 0xffff))
(assert_return (invoke "i32.atomic.load16_u" (i32.const 6)) (i32.const 0xffff))

(assert_return (invoke "i64.atomic.load8_u" (i32.const 0)) (i64.const 0xff))
(assert_return (invoke "i64.atomic.load8_u" (i32.const 5)) (i64.const 0xff))

(assert_return (invoke "i64.atomic.load16_u" (i32.const 0)) (i64.const 0xffff))
(assert_return (invoke "i64.atomic.load16_u" (i32.const 6)) (i64.const 0xffff))

(assert_return (invoke "i64.atomic.load32_u" (i32.const 0)) (i64.const 0xffffffff))
(assert_return (invoke "i64.atomic.load32_u" (i32.const 4)) (i64.const 0xffffffff))

;; *.atomic.store*

(invoke "init" (i64.const 0x0))

(assert_return (invoke "i32.atomic.store" (i32.const 0) (i32.const 0xffffffff)))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0x00000000ffffffff))

(assert_return (invoke "i64.atomic.store" (i32.const 0) (i64.const 0xf000000000000000)))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf000000000000000))

(assert_return (invoke "i32.atomic.store8" (i32.const 1) (i32.const 0xff)))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf00000000000ff00))

(assert_return (invoke "i32.atomic.store16" (i32.const 4) (i32.const 0xffff)))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf000ffff0000ff00))

(assert_return (invoke "i64.atomic.store8" (i32.const 0) (i64.const 0xff)))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf000ffff0000ffff))

(assert_return (invoke "i64.atomic.store16" (i32.const 6) (i64.const 0xffff)))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffff0000ffff))

(assert_return (invoke "i64.atomic.store32" (i32.const 4) (i64.const 0xff0000ff)))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xff0000ff0000ffff))

;; *.atomic.rmw*.add

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i32.atomic.rmw.add" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xffffffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xfffffffffffffffe))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw.add" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xfffffffffffffffe))

(invoke "init" (i64.const 0x0000000000000001))
(assert_return (invoke "i32.atomic.rmw8.add_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0x01))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0x0000000000000000))
(assert_return (invoke "i32.atomic.rmw8.add_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0x00))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0x00000000000000ff))

(invoke "init" (i64.const 0x00000000000000ff))
(assert_return (invoke "i32.atomic.rmw8.add_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0x00000000000000fe))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i32.atomic.rmw16.add_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xfffffffffffffffe))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw8.add_u" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xfffffffffffffffe))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw16.add_u" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xfffffffffffffffe))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw32.add_u" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xffffffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xfffffffffffffffe))

;; *.atomic.rmw*.sub

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i32.atomic.rmw.sub" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xffffffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffff00000000))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw.sub" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0x0000000000000000))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i32.atomic.rmw8.sub_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffffffffff00))
(assert_return (invoke "i32.atomic.rmw8.sub_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0x00))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffffffffff01))

(invoke "init" (i64.const 0x00000000000000fe))
(assert_return (invoke "i32.atomic.rmw8.sub_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xfe))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0x00000000000000ff))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i32.atomic.rmw16.sub_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffffffff0000))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw8.sub_u" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffffffffff00))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw16.sub_u" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffffffff0000))

(invoke "init" (i64.const 0xffffffffffffffff))
(assert_return (invoke "i64.atomic.rmw32.sub_u" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xffffffff))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffffffff00000000))

;; *.atomic.rmw*.and

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw.and" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw.and" (i32.const 0) (i64.const 0xffffffff00000000)) (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f000000000))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.and_u" (i32.const 0) (i32.const 0xffffffff)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.and_u" (i32.const 0) (i32.const 0xffff0000)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f00000))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.and_u" (i32.const 0) (i64.const 0xffffffffffffffff)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.and_u" (i32.const 0) (i64.const 0xffffffffffffff00)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f000))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.and_u" (i32.const 0) (i64.const 0xffffffffffff0000)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f00000))

;; *.atomic.rmw*.or

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw.or" (i32.const 0) (i32.const 0xfffffff1)) (i32.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0fffffff1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw.or" (i32.const 0) (i64.const 0xffffffff00000001)) (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xfffffffff0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.or_u" (i32.const 0) (i32.const 0xfffffff1)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.or_u" (i32.const 0) (i32.const 0xffff0001)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.or_u" (i32.const 0) (i64.const 0xfffffffffffffff1)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.or_u" (i32.const 0) (i64.const 0xfffffffffffff1f1)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f1f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.or_u" (i32.const 0) (i64.const 0xffffffffffff0001)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0fffff0f1))

;; *.atomic.rmw*.xor

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw.xor" (i32.const 0) (i32.const 0xfffffff1)) (i32.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f00f0f0f01))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw.xor" (i32.const 0) (i64.const 0xffffffff00000001)) (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0x0f0f0f0ff0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.xor_u" (i32.const 0) (i32.const 0xfffffff1)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f001))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.xor_u" (i32.const 0) (i32.const 0x00000001)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.xor_u" (i32.const 0) (i32.const 0xffff0001)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.xor_u" (i32.const 0) (i32.const 0xffffff01)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f00ff1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.xor_u" (i32.const 0) (i64.const 0xfffffffffffffff1)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f001))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.xor_u" (i32.const 0) (i64.const 0x00000001)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.xor_u" (i32.const 0) (i64.const 0xffff0001)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.xor_u" (i32.const 0) (i64.const 0xffffff01)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f00ff1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.xor_u" (i32.const 0) (i64.const 0xffffffffffff0001)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f00f0ff0f1))

;; *.atomic.rmw*.xchg

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw.xchg" (i32.const 0) (i32.const 0xfffffff1)) (i32.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0fffffff1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw.xchg" (i32.const 0) (i64.const 0xffff0000ffff0000)) (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xffff0000ffff0000))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.xchg_u" (i32.const 0) (i32.const 0xfffffff1)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.xchg_u" (i32.const 0) (i32.const 0xfffffef1)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0fef1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.xchg_u" (i32.const 0) (i64.const 0xfffffffffffffff1)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.xchg_u" (i32.const 0) (i64.const 0xfffffffffffffef1)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0fef1))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.xchg_u" (i32.const 0) (i64.const 0xfffffffffefef0f1)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0fefef0f1))

;; *.atomic.rmw*.cmpxchg (compare false)

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw.cmpxchg" (i32.const 0) (i32.const 0) (i32.const 0x12345678)) (i32.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw.cmpxchg" (i32.const 0) (i64.const 0) (i64.const 0x0101010102020202)) (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.cmpxchg_u" (i32.const 0) (i32.const 0) (i32.const 0xfffffff0)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.cmpxchg_u" (i32.const 0) (i32.const 0) (i32.const 0x000001f0)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.cmpxchg_u" (i32.const 0) (i32.const 0) (i32.const 0xfffffff0)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.cmpxchg_u" (i32.const 0) (i32.const 0) (i32.const 0x0001f0f0)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.cmpxchg_u" (i32.const 0) (i64.const 0) (i64.const 0xfffffffffffffff0)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.cmpxchg_u" (i32.const 0) (i64.const 0) (i64.const 0x00000000000001f0)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.cmpxchg_u" (i32.const 0) (i64.const 0) (i64.const 0xfffffffffffff0f0)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.cmpxchg_u" (i32.const 0) (i64.const 0) (i64.const 0x000000000001f0f0)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.cmpxchg_u" (i32.const 0) (i64.const 0) (i64.const 0xfffffffff0f0f0f0)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.cmpxchg_u" (i32.const 0) (i64.const 0) (i64.const 0x00000001f0f0f0f0)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.cmpxchg_u" (i32.const 0) (i64.const 0xf0f0f0f0f0f0f0f0) (i64.const 0x0000000000000011)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.cmpxchg_u" (i32.const 0) (i64.const 0xf0f0f0f0f0f0f0f0) (i64.const 0x0000000000001111)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.cmpxchg_u" (i32.const 0) (i64.const 0xf0f0f0f0f0f0f0f0) (i64.const 0x0000000011111111)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.cmpxchg_u" (i32.const 0) (i32.const 0xf0f0f0f0) (i32.const 0x00000011)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.cmpxchg_u" (i32.const 0) (i32.const 0xf0f0f0f0) (i32.const 0x00001111)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0f0))

;; *.atomic.rmw*.cmpxchg (compare true)

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw.cmpxchg" (i32.const 0) (i32.const 0xf0f0f0f0) (i32.const 0xf2345678)) (i32.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f2345678))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw.cmpxchg" (i32.const 0) (i64.const 0xf0f0f0f0f0f0f0f0) (i64.const 0xf101010102020202)) (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf101010102020202))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw8.cmpxchg_u" (i32.const 0) (i32.const 0xf0) (i32.const 0xfefefefe)) (i32.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0fe))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i32.atomic.rmw16.cmpxchg_u" (i32.const 0) (i32.const 0xf0f0) (i32.const 0xfefefefe)) (i32.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0fefe))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw8.cmpxchg_u" (i32.const 0) (i64.const 0xf0) (i64.const 0xfefefefefefefefe)) (i64.const 0xf0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0f0fe))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw16.cmpxchg_u" (i32.const 0) (i64.const 0xf0f0) (i64.const 0xfefefefefefefefe)) (i64.const 0xf0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0f0f0fefe))

(invoke "init" (i64.const 0xf0f0f0f0f0f0f0f0))
(assert_return (invoke "i64.atomic.rmw32.cmpxchg_u" (i32.const 0) (i64.const 0xf0f0f0f0) (i64.const 0xfefefefefefefefe)) (i64.const 0xf0f0f0f0))
(assert_return (invoke "i64.atomic.load" (i32.const 0)) (i64.const 0xf0f0f0f0fefefefe))
