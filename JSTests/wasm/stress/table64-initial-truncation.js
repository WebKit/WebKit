//@ skip if $addressBits <= 32
import * as assert from "../assert.js";

function leb128(value) {
    const bytes = [];
    let n = BigInt(value);
    do {
        let byte = Number(n & 0x7fn);
        n >>= 7n;
        if (n !== 0n)
            byte |= 0x80;
        bytes.push(byte);
    } while (n !== 0n);
    return bytes;
}

function moduleBytesWithTableInitial(initial) {
    const tableEntry = [
        0x70,               // funcref element type
        0x04,               // limits flags: bit 2 => 64-bit index type, no maximum
        ...leb128(initial), // initial size (uint64 on the wire)
    ];
    const tableSectionBody = [0x01, ...tableEntry]; // 1 table
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic + version
        0x04, tableSectionBody.length, ...tableSectionBody, // table section
    ]);
}

const twoPow32 = 0x1_0000_0000n;

const truncatingInitials = [
    twoPow32,          // -> 0
    twoPow32 + 5n,     // -> 5
    2n * twoPow32 + 7n // -> 7
];

for (const initial of truncatingInitials) {
    const bytes = moduleBytesWithTableInitial(initial);

    // The module is well-formed, so compilation must succeed.
    const module = new WebAssembly.Module(bytes);
    assert.isObject(module, `expected a Module for initial=${initial}`);

    // But instantiation must reject the impossible table size rather than
    // silently building a truncated one.
    assert.throws(
        () => new WebAssembly.Instance(module),
        WebAssembly.LinkError,
        "couldn't create Table",
    );
}
