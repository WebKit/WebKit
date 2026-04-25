import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

let wat = `(module
    (type $vec (array i32))

    (func $new (export "new") (param i32) (result (ref $vec))
        i32.const 0
        local.get 0
        array.new $vec
    )
)`

let instance = await instantiate(wat);
let ui32Max = 0xFFFFFFFF;

// Prime the LocalAllocator so we hit the bump path rather than going straight to the slow path.
instance.exports.new(2);
for (let i = 0; i < 1000; ++i) {
    try {
        instance.exports.new(ui32Max - i);
    } catch {
        continue;
    }
    throw new Error("allocation shouldn't succeed");
}