(module
  (func $fn (export "fn54") (result funcref)
    loop (result funcref)
      i32.const 0
      try (param i32)
        drop
      end
      ref.null func
    end
    i64.const 0xBADBEEF
    drop
  )
)