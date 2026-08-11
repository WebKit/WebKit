//@ requireOptions("--useWasmJSTypes=1")
//@ memoryHog!
//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// A memory64 may declare a maximum anywhere in its 2**48-page address space, well past what any port
// could map. What its buffer reports is bounded twice over: by the largest representable ArrayBuffer
// byte length, and by the address space one reservation may claim on this platform, so that a resize
// within maxByteLength never fails deterministically.

const options = { memory64: true, threads: true };
const pageSize = 65536;
const maxMemory64Pages = 2 ** 48;
const maxRepresentablePages = 262144;
const maxMemory32Pages = 65536;

// A memory with no declared maximum reports its own address type's ceiling, so it discovers what this
// platform will reserve. A memory32's ceiling is reservable in full everywhere.
const ceilingBytes = new WebAssembly.Memory({ initial: 1n, address: "i64" }).toResizableBuffer().maxByteLength;
assert.truthy(ceilingBytes <= maxRepresentablePages * pageSize, `memory64 ceiling ${ceilingBytes} is not a representable byte length`);
assert.truthy(ceilingBytes >= maxMemory32Pages * pageSize, `memory64 ceiling ${ceilingBytes} is below the memory32 ceiling`);
assert.eq(new WebAssembly.Memory({ initial: 1 }).toResizableBuffer().maxByteLength, maxMemory32Pages * pageSize);

// A declared maximum at the largest representable byte length is accepted, and the buffer reports it
// clamped to that ceiling. The shared memory reserves the whole thing up front, so this arm is why the
// file is a memory hog, and a port that cannot spare the address space must refuse cleanly rather than
// fail the test.
for (const shared of [false, true]) {
    let mem;
    try {
        mem = (await instantiate(`
        (module
            (memory (export "mem") i64 1 ${maxRepresentablePages} ${shared ? "shared" : ""}))
        `, {}, options)).exports.mem;
    } catch (e) {
        assert.truthy(e instanceof RangeError && e.message === "Out of memory", `expected an out of memory RangeError, got ${e}`);
        continue;
    }

    assert.eq(mem.type().maximum, BigInt(maxRepresentablePages));
    assert.eq(mem.type().address, "i64");
    assert.eq(mem.toResizableBuffer().maxByteLength, Math.min(maxRepresentablePages * pageSize, ceilingBytes));
}

// A maximum past that is a declaration and nothing more: it is reported back verbatim, and the buffer
// still only advertises what could be reached. A non-shared memory reserves nothing up front, so this
// costs no address space.
{
    const mem = new WebAssembly.Memory({ initial: 1n, maximum: BigInt(maxMemory64Pages), address: "i64" });
    assert.eq(mem.type().maximum, BigInt(maxMemory64Pages));
    assert.eq(mem.toResizableBuffer().maxByteLength, ceilingBytes);
}

// The JS constructor applies the same bound. Module decoding is covered by
// memory64-oversized-limits.js.
assert.throws(() => new WebAssembly.Memory({ initial: 1n, maximum: BigInt(maxMemory64Pages) + 1n, address: "i64" }),
    RangeError, "WebAssembly.Memory 'maximum' page count is too large");

// A memory32 is bounded by the memory32 page limit rather than the memory64 one.
assert.throws(() => new WebAssembly.Memory({ initial: 1, maximum: maxMemory32Pages + 1 }),
    RangeError, "WebAssembly.Memory 'maximum' page count is too large");
