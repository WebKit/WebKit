(module
    (table $table (export "table") 3 anyfunc)
    (func $f0 (result i32) i32.const 42)
    (func $f1 (result i32) i32.const 83)
    (elem (i32.const 0) $f0 $f1))
