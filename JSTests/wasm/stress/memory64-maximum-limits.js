//@ requireOptions("--useWasmJSTypes=1")
//@ memoryHog!
//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// A memory64's declared maximum is bounded so that it is always representable as an ArrayBuffer byte
// length. What its buffer reports is bounded further, by the address space one reservation may claim on
// this platform, so that a resize within maxByteLength never fails deterministically.

const options = { memory64: true, threads: true };
const pageSize = 65536;
const maxMemory64Pages = 262144;
const maxMemory32Pages = 65536;

// A memory with no declared maximum reports its own address type's ceiling, so it discovers what this
// platform will reserve. A memory32's ceiling is reservable in full everywhere.
const ceilingBytes = new WebAssembly.Memory({ initial: 1n, address: "i64" }).toResizableBuffer().maxByteLength;
assert.truthy(ceilingBytes <= maxMemory64Pages * pageSize, `memory64 ceiling ${ceilingBytes} exceeds the declarable maximum`);
assert.truthy(ceilingBytes >= maxMemory32Pages * pageSize, `memory64 ceiling ${ceilingBytes} is below the memory32 ceiling`);
assert.eq(new WebAssembly.Memory({ initial: 1 }).toResizableBuffer().maxByteLength, maxMemory32Pages * pageSize);

// A declared maximum at the limit is accepted, and the buffer reports it clamped to that ceiling. The
// shared memory reserves the whole thing up front, so this arm is why the file is a memory hog, and a
// port that cannot spare the address space must refuse cleanly rather than fail the test.
for (const shared of [false, true]) {
    let mem;
    try {
        mem = (await instantiate(`
        (module
            (memory (export "mem") i64 1 ${maxMemory64Pages} ${shared ? "shared" : ""}))
        `, {}, options)).exports.mem;
    } catch (e) {
        assert.truthy(e instanceof RangeError && e.message === "Out of memory", `expected an out of memory RangeError, got ${e}`);
        continue;
    }

    assert.eq(mem.type().maximum, BigInt(maxMemory64Pages));
    assert.eq(mem.type().address, "i64");
    assert.eq(mem.toResizableBuffer().maxByteLength, Math.min(maxMemory64Pages * pageSize, ceilingBytes));
}

// The JS constructor applies the same bound. Module decoding is covered by
// memory64-oversized-limits.js.
assert.throws(() => new WebAssembly.Memory({ initial: 1n, maximum: BigInt(maxMemory64Pages + 1), address: "i64" }),
    RangeError, "WebAssembly.Memory 'maximum' page count is too large");

// A memory32 is bounded by the memory32 page limit rather than the memory64 one.
assert.throws(() => new WebAssembly.Memory({ initial: 1, maximum: maxMemory32Pages + 1 }),
    RangeError, "WebAssembly.Memory 'maximum' page count is too large");
