(module
  (func (export "test") (result f64)
    f64.const 42
    f64.const 43
    i32.const -1
    i32.const -2
    i32.eq
    select
))
