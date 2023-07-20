load("../libwabt.js")

let prologue = `
(module
    (func (export "fib") (param i32) (result i32)
        (local i32 i32)
`

let wat = `
        (i32.const 1)
        (local.set 1)
        (loop
            (local.get 2)
            (local.get 1)
            (local.get 2)
            (i32.add)
            (local.set 2)
            (local.set 1)
            (local.get 0)
            (i32.const 1)
            (i32.sub)
            (local.tee 0)
            (i32.const 0)
            (i32.gt_s)
            (br_if 0)
        )
`

let postlogue = `
        (local.get 2)
        (return)
    )
)
`

const wabtModule = await WabtModule();
let s = prologue;
for (let i = 0; i < 100000; ++i) {
    s += wat;
}
s += postlogue;
const binary = wabtModule.parseWat("filenamesAreCool", s, {}).toBinary({}).buffer;

write('ipint-stress.wasm', binary);
