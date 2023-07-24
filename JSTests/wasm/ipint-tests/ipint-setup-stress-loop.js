load("../libwabt.js")

let wabt = `
(module
    (func (export "loopy") (result i32)
        (local i32 i32 i32)
        (loop
            (loop
                (local.get 0)
                (local.get 1)
                (i32.mul)
                (local.get 2)
                (i32.add)
                (local.set 2)
                (local.get 1)
                (i32.const 1)
                (i32.add)
                (local.tee 1)
                (i32.const 10000)
                (i32.lt_s)
                (br_if 0)
            )
            (local.get 0)
            (i32.const 1)
            (i32.add)
            (local.tee 0)
            (i32.const 10000)
            (i32.lt_s)
            (br_if 0)
        )
        (local.get 2)
    )
)
`

const wabtModule = await WabtModule();
const binary = wabtModule.parseWat("filenamesAreCool", wabt, {}).toBinary({}).buffer;
write('ipint-stress-loop.wasm', binary);
