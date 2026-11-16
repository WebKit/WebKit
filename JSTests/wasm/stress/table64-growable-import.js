//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// A module whose imported table declares initial == maximum is compiled on the
// assumption that the table can never be resized: BBQ and OMG fold its length
// into a constant and treat its function buffer as immutable. Comparing the
// declared and the provided maximum as u32 let a table64 whose real maximum is
// above 2^32 satisfy such an import and then grow underneath compiled code.
{
    const table = new WebAssembly.Table({ element: "funcref", initial: 5n, maximum: 5n, address: "i64" });
    const growable = new WebAssembly.Table({ element: "funcref", initial: 5n, maximum: 4294967301n, address: "i64" });
    const wat = `
    (module
        (import "m" "t" (table $t i64 5 5 funcref))
    )`;

    await instantiate(wat, { m: { t: table } }, { memory64: true });
    await assert.throwsAsync(
        instantiate(wat, { m: { t: growable } }, { memory64: true }),
        WebAssembly.LinkError,
        "Imported Table m:t 'maximum' is larger than the module's expected 'maximum'");
}

// A table whose maximum genuinely matches the declared one stays resizable, and
// every tier must see the length it grew to rather than the declared minimum.
{
    const helper = await instantiate(`
    (module
        (func (export "f") (result i32)
            (i32.const 42)
        )
    )`, {}, {});

    const table = new WebAssembly.Table({ element: "funcref", initial: 5n, maximum: 4294967301n, address: "i64" });
    const instance = await instantiate(`
    (module
        (import "m" "t" (table $t i64 5 4294967301 funcref))
        (type $ft (func (result i32)))
        (func (export "callAt") (param $i i64) (result i32)
            (local.get $i)
            (call_indirect $t (type $ft))
        )
    )`, { m: { t: table } }, { memory64: true });

    table.grow(5n);
    table.set(7n, helper.exports.f);
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(instance.exports.callAt(7n), 42);
}
