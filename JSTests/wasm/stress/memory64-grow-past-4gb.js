//@ memoryHog!
//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// A memory64 exists to address more than the 4GiB a memory32 can, so both the wasm-level grow and
// the resizable-buffer path must cross that boundary.

const options = { memory64: true, threads: true };
const pageSize = 65536;
const maxMemory64Pages = 262144;
const fourGiB = 4 * 1024 * 1024 * 1024;
const pastFourGiB = fourGiB + pageSize;

// A memory with no declared maximum reports the most this platform could grow it to, which is what
// bounds every maxByteLength below.
const ceilingBytes = new WebAssembly.Memory({ initial: 1n, address: "i64" }).toResizableBuffer().maxByteLength;

// A memory whose initial size is already past 4GiB proves this port can host one at all. Where it
// can, growing past 4GiB below must succeed; where it cannot, refusing cleanly is the only option.
// Only the constructor belongs in the try: an assertion inside it would be swallowed by the catch.
let probe;
try {
    probe = new WebAssembly.Memory({ initial: BigInt(pastFourGiB / pageSize), address: "i64" });
} catch (e) {
    assert.truthy(e instanceof RangeError && e.message === "Out of memory", `expected an out of memory RangeError, got ${e}`);
}
const canHostPastFourGiB = probe !== undefined;
if (canHostPastFourGiB)
    assert.eq(probe.buffer.byteLength, pastFourGiB);
// Nothing below needs the probe, and holding 4GiB across the rest of the file would starve it.
probe = undefined;

for (const shared of [false, true]) {
    let mem;
    try {
        mem = (await instantiate(`
        (module
            (memory (export "mem") i64 1 ${pastFourGiB / pageSize} ${shared ? "shared" : ""}))
        `, {}, options)).exports.mem;
    } catch (e) {
        // A shared memory reserves its whole maximum up front, which this port may not be able to spare.
        assert.truthy(e instanceof RangeError && e.message === "Out of memory", `expected an out of memory RangeError, got ${e}`);
        continue;
    }

    const buffer = mem.toResizableBuffer();
    assert.eq(buffer.maxByteLength, Math.min(pastFourGiB, ceilingBytes));

    const grow = shared ? size => buffer.grow(size) : size => buffer.resize(size);
    if (canHostPastFourGiB) {
        // The non-shared path grows by copying, so it holds the old and new regions at once. Hosting one
        // 4GiB memory does not imply room for both.
        let grown = true;
        try {
            grow(pastFourGiB);
        } catch (e) {
            assert.truthy(e instanceof RangeError, `expected a RangeError, got ${e}`);
            grown = false;
        }
        if (grown)
            assert.eq(buffer.byteLength, pastFourGiB);
    } else {
        assert.throws(() => grow(pastFourGiB), RangeError, "failed with new byte length");
        assert.eq(buffer.byteLength, pageSize);
    }

    // Past the declared maximum is a clean RangeError either way. The shared and non-shared paths
    // word it differently ("grow failed ...", "ArrayBuffer resize failed ...") around a common core.
    assert.throws(() => grow(pastFourGiB + pageSize), RangeError, "failed with new byte length");
}

// A shared memory's buffer reports the maximum it can actually reach, and growth within the mapping
// succeeds.
{
    let memory;
    try {
        memory = new WebAssembly.Memory({ initial: 1n, maximum: BigInt(maxMemory64Pages), address: "i64", shared: true });
    } catch (e) {
        assert.truthy(e instanceof RangeError && e.message === "Out of memory", `expected an out of memory RangeError, got ${e}`);
    }
    if (memory !== undefined) {
        const buffer = memory.toResizableBuffer();
        assert.eq(buffer.maxByteLength, Math.min(maxMemory64Pages * pageSize, ceilingBytes));
        buffer.grow(2 * pageSize);
        assert.eq(buffer.byteLength, 2 * pageSize);
    }
}
