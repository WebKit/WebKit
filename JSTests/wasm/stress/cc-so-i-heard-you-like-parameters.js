//@ requireOptions("--useInterpretedJSEntryWrappers=1")
//  Debugging: jsc -m cc-int-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --useInterpretedJSEntryWrappers=1 --dumpDisassembly=0
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// setup and base case
let wat = `
(module
    (func $psel2 (export "psel2") (param $req1 i32) (param $req2 i32) (result i32)
        (if (result i32) (i32.eq (local.get $req1) (i32.const 1)) (then
            (i32.const 1)
        )
        (else 
            (if (result i32) (i32.eq (local.get $req2) (i32.const 1)) (then
                (i32.const 2)
            ) (else
                (i32.const 0)
            ))
        ))
    )
`

// now we generate
for (let i = 4; i <= 256; i *= 2) {
    wat += `
    (func $psel${i} (export "psel${i}") `
    for (let r = 0; r < i; ++r) {
        wat += `(param $req${r} i32) `
    }
    wat += `(result i32)
        (local $left i32)
        (local $right i32)
        (local.set $left (call $psel${i/2} `
    for (let r = 0; r < i / 2; ++r) {
        wat += `(local.get $req${r}) `
    }
    wat += `))
        (local.set $right (call $psel${i/2} `
    for (let r = i / 2; r < i; ++r) {
        wat += `(local.get $req${r}) `
    }
    wat += `))
        (i32.ne (local.get $left) (i32.const 0))
        (if (result i32) (then
            (local.get $left)
        ) (else
            (i32.ne (local.get $right) (i32.const 0))
            (if (result i32) (then
                (i32.add (local.get $right) (i32.const ${i/2}))
            ) (else
                (i32.const 0)
            ))
        ))
    )
`
}

wat += `
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { psel2, psel4, psel256 } = instance.exports

    assert.eq(psel2(0, 0), 0);
    assert.eq(psel2(1, 0), 1);
    assert.eq(psel2(0, 1), 2);
    assert.eq(psel2(1, 1), 1);
    assert.eq(psel4(1, 0, 1, 0), 1);

    let args = [];
    for (let i = 0; i < 256; ++i) {
        if (i === 18 || i === 32 || i === 47 || i === 48 || i === 125 || i === 201) {
            args.push(1);
        } else {
            args.push(0);
        }
    }

    assert.eq(psel256(...args), 19);
}

await assert.asyncTest(test())
