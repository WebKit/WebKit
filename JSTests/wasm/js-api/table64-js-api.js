//@ requireOptions("--useWasmJSTypes=true")
//@ runDefaultWasm("-m", "--useWasmMemory64=1")
import * as assert from "../assert.js";

{
    const table = new WebAssembly.Table({ initial: 1n, address: "i64", element: "externref" });
    assert.eq(table.type().minimum, 1n);
    assert.eq(table.type().address, "i64");
}

{
    const table = new WebAssembly.Table({ initial: 1, address: "i32", element: "externref" });
    assert.eq(table.type().minimum, 1);
    assert.eq(table.type().address, "i32");
}

{
    assert.throws(
        () => new WebAssembly.Table({ initial: 1, address: "i64", element: "externref" }),
        TypeError,
        "Invalid argument type in ToBigInt operation"
    )
}

{
    const table = new WebAssembly.Table({ initial: 1n, maximum: 10n, address: "i64", element: "externref" });
    assert.eq(table.type().maximum, 10n);
    assert.eq(table.type().address, "i64");
}

{
    const table = new WebAssembly.Table({ initial: 1, maximum: 10, address: "i32", element: "externref" });
    assert.eq(table.type().maximum, 10);
    assert.eq(table.type().address, "i32");
}

{
    const table = new WebAssembly.Table({ initial: 1, element: "externref"});
    assert.eq(table.type().minimum, 1);
    assert.eq(table.type().address, "i32");
}

{
    assert.throws(
        () => new WebAssembly.Table({ initial: 1n, maximum: 10, address: "i64", element: "externref" }),
        TypeError,
        "Invalid argument type in ToBigInt operation"
    )
}

{
    const table = new WebAssembly.Table({ initial: 1n, maximum: 10n, address: "i64", element: "externref" });

    const copy = new WebAssembly.Table(table.type());
    assert.eq(copy.type().minimum, 1n);
    assert.eq(copy.type().maximum, 10n);
    assert.eq(copy.type().address, "i64");
    assert.eq(copy.type().element, "externref");
}

{
    assert.throws(
        () => new WebAssembly.Table({initial: 10000001n, maximum: 0n, address: "i64", element: "funcref"}),
        RangeError,
        "WebAssembly.Table 'initial' value is above the upper bound 10000000"
    );
}

{
    // This should not throw
    new WebAssembly.Table({initial: BigInt(2**20), maximum: BigInt(2**64) - 1n, element: "funcref", address: "i64"});
}

{
    const table = new WebAssembly.Table({element: "funcref", initial: 20n, maximum: 30n, address: "i64"});
    assert.eq(20n, table.grow(0n));
    assert.eq(20, table.length);
    assert.eq(20n, table.grow(1n));
    assert.eq(21, table.length);
}

{
    const table = new WebAssembly.Table({element: "funcref", initial: 20n, maximum: 30n, address: "i64"});
    assert.eq(20n, table.grow(10n));
    assert.eq(30n, table.grow(0n));
    assert.throws(() => table.grow(1n), RangeError, "WebAssembly.Table.prototype.grow could not grow the table");
}

{
    const table = new WebAssembly.Table({element: "funcref", initial: 20n, address: "i64"});
    let called = false;
    table.grow({valueOf() { called = true; return 42n; }});
    assert.truthy(called);
    assert.eq(62, table.length);
}

{
    const table = new WebAssembly.Table({element: "funcref", initial: 20n, address: "i64"});
    assert.throws(() => table.get(20n), RangeError, "WebAssembly.Table.prototype.get expects an integer less than the length of the table");
    for (let i = 0; i < 20; i++)
        assert.eq(table.get(BigInt(i)), null);
}

{
    const table = new WebAssembly.Table({element: "funcref", initial: 20n, address: "i64"});
    assert.throws(() => table.set(20n, null), RangeError, "WebAssembly.Table.prototype.set expects an integer less than the length of the table");
    for (let i = 0; i < 20; i++)
        table.set(BigInt(i), null);
}

