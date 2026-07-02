(module
    (import "./global.js" "count" (global (mut i32)))
    (func (export "incrementCount")
        (global.set 0
            (i32.add
                (global.get 0)
                (i32.const 1)))))
