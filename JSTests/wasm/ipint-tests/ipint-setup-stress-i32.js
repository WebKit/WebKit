load("../libwabt.js")

// admit it it's a good function name
let prologue = `
(module
    (func (export "INTerpret") (result i32)
        (i32.const 0)
`
let postlogue = `
    )
)
`

const CHANCE_ABOVE = 0.1
const CUTOFF = 128
const MAX_INT = 1048576

const wabtModule = await WabtModule();
let s = prologue;
for (let i = 0; i < 100000; ++i) {
    let x = '';
    if (Math.random() < CHANCE_ABOVE) {
        x = `
            (i32.const ${Math.floor(Math.random() * (MAX_INT - CUTOFF)) + CUTOFF})
            (i32.add)
        `
    } else {
        x = `
            (i32.const ${Math.floor(Math.random() * (CUTOFF))})
            (i32.add)
        `
    }
    s += x;
}
s += postlogue;
const binary = wabtModule.parseWat("filenamesAreCool", s, {}).toBinary({}).buffer;
write('ipint-stress-i32.wasm', binary);
