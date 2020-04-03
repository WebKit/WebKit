//@ skip if $architecture != "x86-64"
//@ requireOptions("--useWebAssemblyFastMemory=true")
// FIXME: Stop skipping when we enable fast memory for iOS. https://bugs.webkit.org/show_bug.cgi?id=170774

import { instantiate } from '../wabt-wrapper.js';

const instance = instantiate(`
    (module

    (memory 0)

    (func $grow
        (memory.grow (i32.const 1))
        (drop)
    )

    (func $f (param $bail i32)
        (br_if 0 (local.get $bail))
        (call $grow)
        (i32.store (i32.const 42) (i32.const 0))
    )

    (func (export "main")
        (local $i i32)
        (local.set $i (i32.const 100000))
        (loop
            (i32.sub (local.get $i) (i32.const 1))
            (local.tee $i)
            (call $f (i32.const 1))
            (br_if 0)
        )
        (call $f (i32.const 0))
    )

    )
`);


// This should not throw an OutOfBounds exception
instance.exports.main();
