//@ runDefault("--useWasmCustomPageSizes=1")

// A shared memory reserves its declared maximum up front, so a compiler may treat its base and
// bounds-checking size as fixed and materialize them as constants. That is only sound while the
// bounds value cannot move, which growing a memory checked against its own size does.
//
// This runs as a script rather than an ES module: the call has to reach the tier that bakes those
// constants, and top-level module code does not get there.

function varuint(value) {
    value = BigInt(value);
    const bytes = [];
    do {
        let byte = Number(value & 0x7fn);
        value >>= 7n;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, bytes) {
    return [id, ...varuint(bytes.length), ...bytes];
}

const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
const i32 = 0x7f;

// One shared memory with a maximum, and an i32.load8_u of a dynamic address.
function sharedMemoryModule(log2PageSize) {
    const limits = log2PageSize === undefined
        ? [0x03, 0x01, 0x04]
        : [0x0b, 0x01, 0x04, log2PageSize];
    const types = [0x01, 0x60, 0x01, i32, 0x01, i32];
    const body = [0x00, 0x20, 0x00, 0x2d, 0x00, 0x00, 0x0b];
    return new Uint8Array([
        ...header,
        ...section(0x01, types),
        ...section(0x03, [0x01, 0x00]),
        ...section(0x05, [0x01, ...limits]),
        ...section(0x07, [0x02, 0x04, 0x6c, 0x6f, 0x61, 0x64, 0x00, 0x00, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00]),
        ...section(0x0a, [0x01, body.length, ...body]),
    ]);
}

function traps(load, address) {
    try {
        load(address);
    } catch (e) {
        if (e instanceof WebAssembly.RuntimeError)
            return;
        throw new Error(`expected a RuntimeError at ${address}, got ${e}`);
    }
    throw new Error(`expected an out-of-bounds trap at ${address}`);
}

function checkGrowStaysBounded(log2PageSize) {
    const instance = new WebAssembly.Instance(new WebAssembly.Module(sharedMemoryModule(log2PageSize)));
    const load = address => instance.exports.load(address);

    for (let i = 0; i < 200000; ++i)
        load(0);

    const before = instance.exports.mem.buffer.byteLength;
    traps(load, before);

    instance.exports.mem.grow(1);
    const after = instance.exports.mem.buffer.byteLength;
    if (!(after > before))
        throw new Error("memory did not grow");

    // Reachable only if the grown size reached the compiled code, and trapping only if the
    // value it checks against is not stale.
    load(after - 1);
    traps(load, after);
}

// FIXME: A shared memory cannot yet have a page size other than the default, because its
// reservation and growth work in OS pages. Add 0 here once that lands.
checkGrowStaysBounded(16);
checkGrowStaysBounded(undefined);
