//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=0")

import * as assert from "../assert.js";

// Validation of the page size exponent, and of limits that are relative to the
// page size: a memory's declared pages must span at most its address space.

const customPageSizesEnabled = !!jscOptions().useWasmCustomPageSizes;

const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];

function section(id, bytes) {
    return [id, ...varuint(bytes.length), ...bytes];
}

// LEB128, which both the section length and the limits fields are encoded in.
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

const hasMax = 0x01;
const isMemory64 = 0x04;
const hasPageSize = 0x08;

// One memory, built from its flags and whichever limits fields they select.
function memoryModule({ flags = 0, initial = 0, maximum, log2PageSize }) {
    const limits = [flags, ...varuint(initial)];
    if (maximum !== undefined)
        limits.push(...varuint(maximum));
    if (log2PageSize !== undefined)
        limits.push(...varuint(log2PageSize));
    return new Uint8Array([...header, ...section(0x05, [0x01, ...limits])]);
}

function validates(options) {
    new WebAssembly.Module(memoryModule(options));
}

function failsToValidate(options, message) {
    assert.throws(() => new WebAssembly.Module(memoryModule(options)), WebAssembly.CompileError, message);
}

// A memory that states no page size is unaffected by the feature, so its
// limits must keep validating exactly as they do today, flag on or off.
validates({ initial: 65536 });
failsToValidate({ initial: 65537 }, "initial page count");
validates({ flags: hasMax, initial: 0, maximum: 65536 });
failsToValidate({ flags: hasMax, initial: 0, maximum: 65537 }, "maximum page count");
validates({ flags: isMemory64, initial: 0x1000000000000 });
failsToValidate({ flags: isMemory64, initial: 0x1000000000001 }, "initial page count");

// With the feature off the page size byte is rejected outright, so none of the
// checks below apply.
if (!customPageSizesEnabled) {
    failsToValidate({ flags: hasPageSize, log2PageSize: 0 }, "custom page sizes are not enabled");
    failsToValidate({ flags: hasPageSize, log2PageSize: 16 }, "custom page sizes are not enabled");
}

if (customPageSizesEnabled) {
    // The two valid page sizes, 2**0 and 2**16.
    for (const log2PageSize of [0, 16]) {
        validates({ flags: hasPageSize, log2PageSize });
        validates({ flags: hasPageSize | hasMax, initial: 0, maximum: 1, log2PageSize });
        validates({ flags: hasPageSize | isMemory64, log2PageSize });
    }

    // Every other exponent the parser accepts is invalid. The spec test enumerates
    // pagesize 2 through 32768 plus 0x20000; this covers those and the rest up to
    // the parser's own limit of 63.
    for (let log2PageSize = 1; log2PageSize < 64; ++log2PageSize) {
        if (log2PageSize === 16)
            continue;
        failsToValidate({ flags: hasPageSize, log2PageSize }, "invalid custom page size");
        failsToValidate({ flags: hasPageSize | isMemory64, log2PageSize }, "invalid custom page size");
    }

    // An exponent of 64 or more is malformed rather than invalid, and is rejected
    // a commit earlier, while parsing.
    failsToValidate({ flags: hasPageSize, log2PageSize: 64 }, "page size 2**64 is too large");

    // i32 limits are bounded by 2**32 / pageSize, and by 2**32 - 1.
    const maxSmallPagesMemory32 = 0xffffffff; // 2**32 - 1 one-byte pages
    validates({ flags: hasPageSize, initial: maxSmallPagesMemory32, log2PageSize: 0 });
    validates({ flags: hasPageSize | hasMax, initial: 0, maximum: maxSmallPagesMemory32, log2PageSize: 0 });
    failsToValidate({ flags: hasPageSize, initial: maxSmallPagesMemory32 + 1, log2PageSize: 0 }, "initial page count");
    failsToValidate({ flags: hasPageSize | hasMax, initial: 0, maximum: maxSmallPagesMemory32 + 1, log2PageSize: 0 },
        "maximum page count");

    // Stating the default page size explicitly must not move that boundary either.
    validates({ flags: hasPageSize, initial: 65536, log2PageSize: 16 });
    failsToValidate({ flags: hasPageSize, initial: 65537, log2PageSize: 16 }, "initial page count");
    validates({ flags: hasPageSize | hasMax, initial: 0, maximum: 65536, log2PageSize: 16 });
    failsToValidate({ flags: hasPageSize | hasMax, initial: 0, maximum: 65537, log2PageSize: 16 },
        "maximum page count");

    // i64 limits are declarative: a memory may declare far more pages than this
    // process could ever map, and must still validate.
    const maxMemory64Pages = 0x1000000000000; // 2**64 / 65536
    for (const log2PageSize of [0, 16]) {
        validates({ flags: hasPageSize | isMemory64, initial: maxMemory64Pages, log2PageSize });
        // FIXME: With a one-byte page size the proposal allows up to 2**64 - 1 pages. A PageCount
        // cannot yet represent a count that large, so maxDeclarablePages clamps both page sizes to
        // the same bound. Raise this once PageCount carries its own page size.
        failsToValidate({ flags: hasPageSize | isMemory64, initial: maxMemory64Pages + 1, log2PageSize },
            "initial page count");
    }

    // Declaring more than can be allocated is valid, and fails at instantiation
    // with an out-of-memory error rather than at validation. This allocates
    // nothing: the failure happens before any memory is reserved.
    {
        const module = new WebAssembly.Module(memoryModule({
            flags: hasPageSize,
            initial: maxSmallPagesMemory32,
            log2PageSize: 0,
        }));
        assert.throws(() => new WebAssembly.Instance(module), Error, "Out of memory");
    }
}
