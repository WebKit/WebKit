function returnYourself() {
    return returnYourself;
}

/*
(module
    (func $imported (import "fun" "a") (result funcref))
    (func $main (export "main")
        (local $do_exit i32)
        i32.const 0
        local.set $do_exit
        (loop $top
            ;; the $proceed block passes on the first iteration and returns from the function the second time it's entered
            (block $proceed
                local.get $do_exit
                i32.eqz
                br_if $proceed
                return
            )
            i32.const 1
            local.set $do_exit
            (try_table $x (catch_all $top)
                call $imported
                drop
            )
            unreachable
        )
    )
)
*/
const module = new WebAssembly.Module(new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60,
  0x00, 0x01, 0x70, 0x60, 0x00, 0x00, 0x02, 0x09, 0x01, 0x03, 0x66, 0x75,
  0x6e, 0x01, 0x61, 0x00, 0x00, 0x03, 0x02, 0x01, 0x01, 0x07, 0x08, 0x01,
  0x04, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x01, 0x0a, 0x28, 0x01, 0x26, 0x01,
  0x01, 0x7f, 0x41, 0x00, 0x21, 0x00, 0x03, 0x40, 0x02, 0x40, 0x20, 0x00,
  0x45, 0x0d, 0x00, 0x0f, 0x0b, 0x41, 0x01, 0x21, 0x00, 0x02, 0x40, 0x1f,
  0x40, 0x01, 0x02, 0x01, 0x10, 0x00, 0x1a, 0x0b, 0x0b, 0x00, 0x0b, 0x00,
  0x0b
]));
const instance = new WebAssembly.Instance(module, {
    fun: {
        a: returnYourself
    }
});

// Will fail with a random failure including but not limited to Unreachable or a segfault
// if the fix https://bugs.webkit.org/show_bug.cgi?id=292599 has regressed.
// Will fail with Unreachable if the exception is not thrown or not caught correctly.
instance.exports.main();
