//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=0")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=0", "--thresholdForBBQOptimizeAfterWarmUp=0")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeAfterWarmUp=10")

import * as assert from "../assert.js";

// Data segments and the bulk memory operations are specified in bytes, so a page size of 1 only
// reaches them through the memory's byte length. Every bound here is that length, exactly, since
// a small-page memory stops short of the OS page its mapping ends on.

const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];

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

function name(text) {
    const bytes = [...text].map(c => c.charCodeAt(0));
    return [...varuint(bytes.length), ...bytes];
}

const i32 = 0x7f;

// Limits bytes. Omitting log2PageSize gives the pre-proposal encoding, meaning 64 KiB.
function limits(initial, log2PageSize, maximum) {
    const hasMax = maximum !== undefined;
    if (log2PageSize === undefined)
        return [hasMax ? 0x01 : 0x00, ...varuint(initial), ...(hasMax ? varuint(maximum) : [])];
    return [hasMax ? 0x09 : 0x08, ...varuint(initial), ...(hasMax ? varuint(maximum) : []), log2PageSize];
}

// A memarg naming a memory other than 0 sets bit 6 of the alignment and follows it with the index.
function memarg(memoryIndex) {
    return memoryIndex ? [0x40, ...varuint(memoryIndex), 0x00] : [0x00, 0x00];
}

function active(memoryIndex, offset, bytes) {
    const offsetExpr = [0x41, ...varuint(offset), 0x0b];
    if (!memoryIndex)
        return { bytes, encoding: [0x00, ...offsetExpr, ...varuint(bytes.length), ...bytes] };
    return { bytes, encoding: [0x02, ...varuint(memoryIndex), ...offsetExpr, ...varuint(bytes.length), ...bytes] };
}

function passive(bytes) {
    return { bytes, encoding: [0x01, ...varuint(bytes.length), ...bytes] };
}

// Builds a module over `memories`, with `segments` in the data section. Each memory is exported as
// "memN" with accessors "loadN"/"storeN"/"sizeN"/"growN"/"fillN". `copies` and `inits` list the
// (destination, source) memory pairs and (segment, memory) pairs to export operations for.
function bulkModule(memories, segments = [], { copies = [], inits = [] } = {}) {
    const types = [
        0x04,
        0x60, 0x01, i32, 0x01, i32,
        0x60, 0x00, 0x01, i32,
        0x60, 0x02, i32, i32, 0x00,
        0x60, 0x03, i32, i32, i32, 0x00,
    ];

    const funcs = [];
    const exports = [];
    const bodies = [];
    const addFunc = (exportName, typeIndex, body) => {
        exports.push(...name(exportName), 0x00, ...varuint(funcs.length));
        funcs.push(typeIndex);
        bodies.push([0x00, ...body, 0x0b]);
    };

    for (let i = 0; i < memories.length; ++i) {
        addFunc(`load${i}`, 0, [0x20, 0x00, 0x2d, ...memarg(i)]);
        addFunc(`size${i}`, 1, [0x3f, ...varuint(i)]);
        addFunc(`grow${i}`, 0, [0x20, 0x00, 0x40, ...varuint(i)]);
        addFunc(`store${i}`, 2, [0x20, 0x00, 0x20, 0x01, 0x3a, ...memarg(i)]);
        addFunc(`fill${i}`, 3, [0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0xfc, 0x0b, ...varuint(i)]);
    }
    for (const [dst, src] of copies)
        addFunc(`copy${dst}_${src}`, 3, [0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0xfc, 0x0a, ...varuint(dst), ...varuint(src)]);
    for (const [segment, memory] of inits)
        addFunc(`init${segment}_${memory}`, 3, [0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0xfc, 0x08, ...varuint(segment), ...varuint(memory)]);

    for (let i = 0; i < memories.length; ++i)
        exports.push(...name(`mem${i}`), 0x02, ...varuint(i));

    const sections = [
        section(0x01, types),
        section(0x03, [...varuint(funcs.length), ...funcs]),
        section(0x05, [...varuint(memories.length), ...memories.flat()]),
        section(0x07, [...varuint(memories.length + funcs.length), ...exports]),
    ];
    // The data count section is what lets a function body name a segment index.
    if (segments.length)
        sections.push(section(0x0c, varuint(segments.length)));
    sections.push(section(0x0a, [...varuint(bodies.length), ...bodies.flatMap(body => [...varuint(body.length), ...body])]));
    if (segments.length)
        sections.push(section(0x0b, [...varuint(segments.length), ...segments.flatMap(s => s.encoding)]));

    return new Uint8Array([...header, ...sections.flat()]);
}

function instantiate(...args) {
    return new WebAssembly.Instance(new WebAssembly.Module(bulkModule(...args)));
}

function traps(fn, ...args) {
    assert.throws(() => fn(...args), WebAssembly.RuntimeError, "Out of bounds memory access");
}

function readAll(exports, memoryIndex = 0) {
    const load = exports[`load${memoryIndex}`];
    const result = [];
    for (let i = 0; i < exports[`mem${memoryIndex}`].buffer.byteLength; ++i)
        result.push(load(i));
    return result;
}

// An operation that traps must leave every byte as it was: the bulk operations bounds-check up
// front rather than writing as far as they can.
function trapsWithoutWriting(exports, fn, ...args) {
    const before = readAll(exports);
    traps(fn, ...args);
    assert.eq(readAll(exports).join(), before.join());
}

const xyz = [0x78, 0x79, 0x7a];

// Active data segments land at their byte offset, whatever the page size scales to.
for (const log2PageSize of [0, 16, undefined]) {
    const pageSize = log2PageSize === 0 ? 1 : 65536;
    const pages = pageSize === 1 ? 10 : 1;
    const exports = instantiate([limits(pages, log2PageSize)], [
        active(0, 0, [0x11, 0x22]),
        active(0, 4, [0xaa, 0xbb, 0xcc]),
    ]).exports;

    assert.eq(exports.size0(), pages);
    assert.eq(exports.mem0.buffer.byteLength, pages * pageSize);
    for (const [address, value] of [[0, 0x11], [1, 0x22], [2, 0], [3, 0], [4, 0xaa], [5, 0xbb], [6, 0xcc], [7, 0]])
        assert.eq(exports.load0(address), value);
}

// A segment ending exactly on the memory's last byte is in bounds; one byte further is a link
// error, reported against the memory's byte length rather than anything rounded up from it.
{
    const exports = instantiate([limits(10, 0)], [active(0, 7, [0xaa, 0xbb, 0xcc])]).exports;
    assert.eq(readAll(exports).join(), [0, 0, 0, 0, 0, 0, 0, 0xaa, 0xbb, 0xcc].join());

    const overflowing = new WebAssembly.Module(bulkModule([limits(10, 0)], [active(0, 8, [0xaa, 0xbb, 0xcc])]));
    assert.throws(() => new WebAssembly.Instance(overflowing), WebAssembly.RuntimeError,
        "Invalid data segment initialization: segment of 3 bytes memory of 10 bytes, at offset 8, segment writes outside of memory");
}

// Segments apply in order and in place, so a failure part way through leaves the ones before it
// written. That is only observable on an imported memory, which outlives the failed instantiation.
{
    const provider = instantiate([limits(10, 0)]).exports;
    const importing = new WebAssembly.Module(new Uint8Array([
        ...header,
        ...section(0x02, [0x01, ...name("m"), ...name("mem"), 0x02, ...limits(10, 0)]),
        ...section(0x0b, [0x02,
            ...active(0, 0, [0x11, 0x22, 0x33]).encoding,
            ...active(0, 8, [0xaa, 0xbb, 0xcc]).encoding]),
    ]));
    assert.throws(() => new WebAssembly.Instance(importing, { m: { mem: provider.mem0 } }), WebAssembly.RuntimeError,
        "segment writes outside of memory");

    assert.eq(readAll(provider).join(), [0x11, 0x22, 0x33, 0, 0, 0, 0, 0, 0, 0].join());
}

// A zero-length segment is in bounds at the end of the memory and out of bounds past it.
{
    instantiate([limits(10, 0)], [active(0, 10, [])]);
    const overflowing = new WebAssembly.Module(bulkModule([limits(10, 0)], [active(0, 11, [])]));
    assert.throws(() => new WebAssembly.Instance(overflowing), WebAssembly.RuntimeError, "segment writes outside of memory");
}

// memory.init from a passive segment, at the boundary in both directions.
{
    const exports = instantiate([limits(10, 0)], [passive([0x11, 0x22, 0x33])], { inits: [[0, 0]] }).exports;
    const init = exports.init0_0;

    init(0, 0, 3);
    assert.eq(readAll(exports).join(), [0x11, 0x22, 0x33, 0, 0, 0, 0, 0, 0, 0].join());

    // Ending exactly at the memory's last byte is in bounds.
    init(7, 0, 3);
    assert.eq(readAll(exports).join(), [0x11, 0x22, 0x33, 0, 0, 0, 0, 0x11, 0x22, 0x33].join());

    trapsWithoutWriting(exports, init, 8, 0, 3);
    trapsWithoutWriting(exports, init, 10, 0, 1);
    // Out of bounds in the segment rather than the memory.
    trapsWithoutWriting(exports, init, 0, 1, 3);
    trapsWithoutWriting(exports, init, 0, 4, 0);

    // A zero-length init at the very end of the memory is in bounds; one past it is not.
    init(10, 0, 0);
    trapsWithoutWriting(exports, init, 11, 0, 0);
}

// memory.fill over a small-page memory.
{
    const exports = instantiate([limits(10, 0)]).exports;
    const fill = exports.fill0;

    fill(0, 0xab, 10);
    assert.eq(readAll(exports).join(), new Array(10).fill(0xab).join());

    fill(3, 0xcd, 4);
    assert.eq(readAll(exports).join(), [0xab, 0xab, 0xab, 0xcd, 0xcd, 0xcd, 0xcd, 0xab, 0xab, 0xab].join());

    fill(5, 0x01, 0);
    assert.eq(exports.load0(5), 0xcd);

    fill(10, 0x01, 0);

    trapsWithoutWriting(exports, fill, 0, 0x5a, 11);
    trapsWithoutWriting(exports, fill, 10, 0x5a, 1);
    trapsWithoutWriting(exports, fill, 11, 0x5a, 0);
}

// memory.copy within a small-page memory, including overlap in both directions.
{
    const exports = instantiate([limits(10, 0)], [], { copies: [[0, 0]] }).exports;
    const copy = exports.copy0_0;
    const reset = () => {
        for (let i = 0; i < 10; ++i)
            exports.store0(i, i + 1);
    };

    reset();
    copy(0, 5, 5);
    assert.eq(readAll(exports).join(), [6, 7, 8, 9, 10, 6, 7, 8, 9, 10].join());

    // Overlapping upwards: the destination is above the source, so the bytes must be read before
    // they are overwritten.
    reset();
    copy(2, 0, 8);
    assert.eq(readAll(exports).join(), [1, 2, 1, 2, 3, 4, 5, 6, 7, 8].join());

    // Overlapping downwards.
    reset();
    copy(0, 2, 8);
    assert.eq(readAll(exports).join(), [3, 4, 5, 6, 7, 8, 9, 10, 9, 10].join());

    copy(0, 0, 10);
    copy(10, 10, 0);
    copy(0, 0, 0);

    trapsWithoutWriting(exports, copy, 0, 0, 11);
    trapsWithoutWriting(exports, copy, 9, 0, 2);
    trapsWithoutWriting(exports, copy, 0, 9, 2);
    trapsWithoutWriting(exports, copy, 11, 0, 0);
    trapsWithoutWriting(exports, copy, 0, 11, 0);
}

// The spec test's copy between memories of different page sizes, with its exact bytes.
if (jscOptions().useWasmMultiMemory) {
    const exports = instantiate([limits(10, 0), limits(1, 16)], [
        active(0, 0, [0x11, 0x22, 0x33, 0x44]),
        active(1, 0, [0x55, 0x66, 0x77, 0x88]),
    ], { copies: [[1, 0], [0, 1]] }).exports;

    assert.eq(exports.mem0.buffer.byteLength, 10);
    assert.eq(exports.mem1.buffer.byteLength, 65536);

    exports.copy1_0(6, 0, 2);
    assert.eq(exports.load1(6), 0x11);
    assert.eq(exports.load1(7), 0x22);

    exports.copy0_1(4, 1, 3);
    assert.eq(exports.load0(4), 0x66);
    assert.eq(exports.load0(5), 0x77);
    assert.eq(exports.load0(6), 0x88);

    // Each side is checked against its own memory's size, so a count that fits the 64 KiB memory
    // but not the small-page one still traps.
    const before = readAll(exports, 1).join();
    traps(exports.copy1_0, 0, 0, 11);
    assert.eq(readAll(exports, 1).join(), before);
    traps(exports.copy0_1, 0, 0, 11);

    // And the reverse: a destination large enough is not enough on its own.
    traps(exports.copy0_1, 0, 0, 65536);
    traps(exports.copy1_0, 0, 0, 65536);
}

// A memory sized to fit inline data, which the text format rounds up to a whole page. With
// one-byte pages that is three bytes exactly, and nothing downstream rounds it any further.
{
    const exports = instantiate([limits(3, 0, 3)], [active(0, 0, xyz)]).exports;
    assert.eq(exports.size0(), 3);
    assert.eq(exports.mem0.buffer.byteLength, 3);
    for (let i = 0; i < 3; ++i)
        assert.eq(exports.load0(i), xyz[i]);
    traps(exports.load0, 3);
    assert.eq(exports.grow0(1), -1);
}
{
    const exports = instantiate([limits(1, 16, 1)], [active(0, 0, xyz)]).exports;
    assert.eq(exports.size0(), 1);
    assert.eq(exports.mem0.buffer.byteLength, 65536);
}

// Growing republishes the size the bulk operations are checked against, including when the growth
// stays inside the mapping the memory was already given.
for (const [delta, expected] of [[1, 4], [70000, 70003]]) {
    const exports = instantiate([limits(3, 0, 100000)]).exports;
    const fill = exports.fill0;

    traps(fill, 0, 0x11, 4);
    assert.eq(exports.grow0(delta), 3);
    assert.eq(exports.size0(), expected);

    fill(expected - 4, 0x11, 4);
    assert.eq(exports.load0(expected - 1), 0x11);
    trapsWithoutWriting(exports, fill, expected - 3, 0x22, 4);
}

// Once hot, the bulk operations are inlined against a cached size rather than calling out, so the
// same boundaries have to hold again in the top tiers.
{
    const exports = instantiate([limits(10, 0)], [passive([0x11, 0x22, 0x33])], { copies: [[0, 0]], inits: [[0, 0]] }).exports;
    const { fill0: fill, copy0_0: copy, init0_0: init } = exports;

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        fill(0, i, 10);
        copy(0, 0, 10);
        init(0, 0, 3);
    }

    assert.eq(readAll(exports).slice(0, 3).join(), [0x11, 0x22, 0x33].join());
    trapsWithoutWriting(exports, fill, 0, 0x5a, 11);
    trapsWithoutWriting(exports, copy, 0, 0, 11);
    trapsWithoutWriting(exports, init, 8, 0, 3);
}

// A byte-at-a-time loop over memory 0 runs as a bulk operation when its guard, which uses the same
// cached size, passes. An out-of-bounds range fails that guard and runs the loop, which writes up to
// the first address it cannot, so the same call must leave different bytes behind than a trapping
// memory.fill does.
{
    // for (; n; --n) mem[d++] = v; and the copy equivalent, in the exact shape the matcher accepts.
    const loopFillBody = [
        0x01, 0x01, i32,
        0x03, 0x40,
        0x20, 0x00, 0x20, 0x01, 0x3a, 0x00, 0x00,
        0x20, 0x00, 0x41, 0x01, 0x6a, 0x21, 0x00,
        0x20, 0x02, 0x41, 0x01, 0x6b, 0x22, 0x02,
        0x0d, 0x00,
        0x0b,
    ];
    const loopCopyBody = [
        0x01, 0x01, i32,
        0x03, 0x40,
        0x20, 0x00, 0x20, 0x01, 0x2d, 0x00, 0x00, 0x3a, 0x00, 0x00,
        0x20, 0x00, 0x41, 0x01, 0x6a, 0x21, 0x00,
        0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01,
        0x20, 0x02, 0x41, 0x01, 0x6b, 0x22, 0x02,
        0x0d, 0x00,
        0x0b,
    ];
    const loadBody = [0x00, 0x20, 0x00, 0x2d, 0x00, 0x00, 0x0b];
    const storeBody = [0x00, 0x20, 0x00, 0x20, 0x01, 0x3a, 0x00, 0x00, 0x0b];
    const bodies = [[...loopFillBody, 0x0b], [...loopCopyBody, 0x0b], loadBody, storeBody];

    const module = new Uint8Array([
        ...header,
        ...section(0x01, [0x03,
            0x60, 0x03, i32, i32, i32, 0x00,
            0x60, 0x01, i32, 0x01, i32,
            0x60, 0x02, i32, i32, 0x00]),
        ...section(0x03, [0x04, 0x00, 0x00, 0x01, 0x02]),
        ...section(0x05, [0x01, ...limits(20, 0)]),
        ...section(0x07, [0x05,
            ...name("loopFill"), 0x00, 0x00,
            ...name("loopCopy"), 0x00, 0x01,
            ...name("load0"), 0x00, 0x02,
            ...name("store0"), 0x00, 0x03,
            ...name("mem0"), 0x02, 0x00]),
        ...section(0x0a, [0x04, ...bodies.flatMap(body => [...varuint(body.length), ...body])]),
    ]);
    const exports = new WebAssembly.Instance(new WebAssembly.Module(module)).exports;
    const { loopFill, loopCopy, store0: store } = exports;
    const reset = () => {
        for (let i = 0; i < 20; ++i)
            store(i, i + 1);
    };

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        loopFill(0, i, 1);
        loopCopy(0, 0, 1);
    }

    reset();
    loopFill(10, 0x7e, 10);
    assert.eq(readAll(exports).join(), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, ...new Array(10).fill(0x7e)].join());

    reset();
    assert.throws(() => loopFill(11, 0x5a, 10), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(readAll(exports).join(), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, ...new Array(9).fill(0x5a)].join());

    reset();
    loopCopy(10, 0, 10);
    assert.eq(readAll(exports).join(), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10].join());

    reset();
    assert.throws(() => loopCopy(11, 0, 10), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(readAll(exports).join(), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 1, 2, 3, 4, 5, 6, 7, 8, 9].join());
}
