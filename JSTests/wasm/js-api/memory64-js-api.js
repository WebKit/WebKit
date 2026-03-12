//@ runDefaultWasm("-m", "--useWasmMemory64=1")
import * as assert from '../assert.js';

const pageSize = 64 * 1024;

// Constructor: basic creation with address: "i64" and BigInt initial
{
    const memory = new WebAssembly.Memory({ initial: 1n, address: "i64" });
    assert.eq(memory.buffer.byteLength, 1 * pageSize);
}

// Constructor: "minimum" is accepted as an alias for "initial"
{
    const memory = new WebAssembly.Memory({ minimum: 2n, address: "i64" });
    assert.eq(memory.buffer.byteLength, 2 * pageSize);
}

// Constructor: specifying both "initial" and "minimum" throws
{
    assert.throws(
        () => new WebAssembly.Memory({ initial: 1n, minimum: 1n, address: "i64" }),
        TypeError,
        "WebAssembly.Memory 'initial' and 'minimum' options are specified at the same time"
    );
}

// Constructor: initial > maximum throws
{
    assert.throws(
        () => new WebAssembly.Memory({ initial: 10n, maximum: 5n, address: "i64" }),
        RangeError,
        "'maximum' page count must be than greater than or equal to the 'initial' page count"
    );
}

// Constructor: passing a number (not BigInt) for initial in memory64 throws
{
    assert.throws(
        () => new WebAssembly.Memory({ initial: 1, address: "i64" }),
        TypeError,
        "Invalid argument type in ToBigInt operation"
    );
}

// Constructor: passing a number for maximum in memory64 throws
{
    assert.throws(
        () => new WebAssembly.Memory({ initial: 1n, maximum: 5, address: "i64" }),
        TypeError,
        "Invalid argument type in ToBigInt operation"
    );
}

// Constructor: initial page count too large throws
{
    assert.throws(
        () => new WebAssembly.Memory({ initial: 65537n, address: "i64" }),
        RangeError,
        "WebAssembly.Memory 'initial' page count is too large"
    );
}

// Constructor: maximum page count too large throws
{
    assert.throws(
        () => new WebAssembly.Memory({ initial: 1n, maximum: 65537n, address: "i64" }),
        RangeError,
        "WebAssembly.Memory 'maximum' page count is too large"
    );
}

// Constructor: passing invalid address
{
    assert.throws(
        () => new WebAssembly.Memory({ initial: 1n, maximum: 5n, address: "not a number" }),
        Error,
        "WebAssembly.Memory 'address' must be a string of value 'i32' or 'i64'"
    );
}

// Constructor: passing toString address
{
    const memory = new WebAssembly.Memory({ initial: 1n, maximum: 5n, address: { toString() { return "i64"; }} });
    assert.eq(memory.type().address, "i64");
}

// type(): shape without maximum has 3 keys: minimum, shared, address
{
    const memory = new WebAssembly.Memory({ initial: 3n, address: "i64" });
    const t = memory.type();
    assert.eq(Object.keys(t).length, 3);
    assert.eq(typeof t.minimum, "bigint");
    assert.eq(t.minimum, 3n);
    assert.eq(t.shared, false);
    assert.eq(t.address, "i64");
}

// type(): shape with maximum has 4 keys: minimum, maximum, shared, address
{
    const memory = new WebAssembly.Memory({ initial: 2n, maximum: 10n, address: "i64" });
    const t = memory.type();
    assert.eq(Object.keys(t).length, 4);
    assert.eq(typeof t.minimum, "bigint");
    assert.eq(typeof t.maximum, "bigint");
    assert.eq(t.minimum, 2n);
    assert.eq(t.maximum, 10n);
    assert.eq(t.shared, false);
    assert.eq(t.address, "i64");
}

// type(): shared memory has shared: true
{
    const memory = new WebAssembly.Memory({ initial: 1n, maximum: 4n, shared: true, address: "i64" });
    const t = memory.type();
    assert.eq(t.shared, true);
    assert.eq(t.address, "i64");
}

// type(): round-trip — descriptor from type() can be passed back to the constructor
{
    const memory = new WebAssembly.Memory({ initial: 5n, maximum: 20n, address: "i64" });
    const memory2 = new WebAssembly.Memory(memory.type());
    const t = memory2.type();
    assert.eq(t.minimum, memory.type().minimum);
    assert.eq(t.maximum, memory.type().maximum);
    assert.eq(t.shared, memory.type().shared);
    assert.eq(t.address, memory.type().address);
}

// type(): memory32 minimum and maximum are numbers, not BigInts
{
    const memory = new WebAssembly.Memory({ initial: 2, maximum: 10 });
    const t = memory.type();
    assert.eq(typeof t.minimum, "number");
    assert.eq(typeof t.maximum, "number");
    assert.eq(t.minimum, 2);
    assert.eq(t.maximum, 10);
}

// grow(): returns previous page count as a BigInt for memory64
{
    const memory = new WebAssembly.Memory({ initial: 1n, maximum: 5n, address: "i64" });
    const prev = memory.grow(2n);
    assert.eq(typeof prev, "bigint");
    assert.eq(prev, 1n);
    assert.eq(memory.buffer.byteLength, 3 * pageSize);
}

// grow(): successive grows track page count correctly
{
    const memory = new WebAssembly.Memory({ initial: 1n, maximum: 4n, address: "i64" });
    assert.eq(memory.grow(1n), 1n);
    assert.eq(memory.grow(1n), 2n);
    assert.eq(memory.grow(1n), 3n);
    assert.eq(memory.buffer.byteLength, 4 * pageSize);
}

// grow(): exceeding maximum throws
{
    const memory = new WebAssembly.Memory({ initial: 1n, maximum: 3n, address: "i64" });
    assert.throws(
        () => memory.grow(10n),
        RangeError,
        "WebAssembly.Memory.grow would exceed the memory's declared maximum size"
    );
    assert.eq(memory.buffer.byteLength, 1 * pageSize);
}

// grow(): passing a number (not BigInt) as delta for memory64 throws
{
    const memory = new WebAssembly.Memory({ initial: 1n, maximum: 5n, address: "i64" });
    assert.throws(
        () => memory.grow(1),
        TypeError,
        "Invalid argument type in ToBigInt operation"
    );
}

// grow(): memory32 grow still returns a number
{
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 5 });
    const prev = memory.grow(1);
    assert.eq(typeof prev, "number");
    assert.eq(prev, 1);
}
