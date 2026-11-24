//@ runDefaultWasm("--useWasmIPInt=1", "--useBBQJIT=0", "--useOMGJIT=0")
// The operand stack height is unbounded per spec, so a branch may have to discard any number of
// values that fits in a function body, including more than 16 bits can count. Cover every kind of
// branch on both sides of that limit.

function uleb(n) {
    const out = [];
    while (true) {
        let b = n & 0x7f;
        n >>>= 7;
        if (n)
            out.push(b | 0x80);
        else {
            out.push(b);
            return out;
        }
    }
}

class Bytes {
    constructor() {
        this.buffer = new Uint8Array(1024);
        this.length = 0;
    }

    ensure(extra) {
        if (this.length + extra <= this.buffer.length)
            return;
        const grown = new Uint8Array(Math.max(this.buffer.length * 2, this.length + extra));
        grown.set(this.buffer.subarray(0, this.length));
        this.buffer = grown;
    }

    push(...values) {
        this.ensure(values.length);
        for (const value of values)
            this.buffer[this.length++] = value;
    }

    append(other) {
        this.ensure(other.length);
        this.buffer.set(other.bytes(), this.length);
        this.length += other.length;
    }

    zeros(n) {
        this.ensure(n * 2);
        for (let i = 0; i < n; ++i) {
            this.buffer[this.length++] = 0x41; // i32.const 0
            this.buffer[this.length++] = 0x00;
        }
    }

    bytes() { return this.buffer.subarray(0, this.length); }
}

function section(out, id, payload) {
    out.push(id, ...uleb(payload.length));
    out.append(payload);
}

const i32 = 0x7f;
const typeVoidToI32 = 0;
const typeI32ToI32 = 1;

// Each of these branches out of a block holding `discard` values below the one it keeps. A value
// pushed before the block is added to the result afterwards, so popping too few or too many values
// changes the result.
//
// i32.const 7
// block (result i32)
//   zeros
//   <branch to the block with one i32>
// end
// i32.add
function branchOutOfBlock(discard, branch) {
    const code = new Bytes();
    code.push(0x41, 7, 0x02, i32);
    code.zeros(discard);
    code.push(...branch, 0x0b, 0x6a);
    return code;
}

function br(discard) {
    // i32.const 42, br 0, unreachable
    return { type: typeVoidToI32, code: branchOutOfBlock(discard, [0x41, 42, 0x0c, 0x00, 0x00]), expected: 49 };
}

function brIf(discard) {
    // i32.const 43, i32.const 1, br_if 0, unreachable
    return { type: typeVoidToI32, code: branchOutOfBlock(discard, [0x41, 43, 0x41, 0x01, 0x0d, 0x00, 0x00]), expected: 50 };
}

function brOnNull(discard) {
    // i32.const 44, ref.null extern, br_on_null 0, unreachable
    return { type: typeVoidToI32, code: branchOutOfBlock(discard, [0x41, 44, 0xd0, 0x6f, 0xd5, 0x00, 0x00]), expected: 51 };
}

// i32.const 7
// block (result i32)            ;; outer: br_table and br both discard `discard` values
//   zeros
//   block (result i32)          ;; inner: discards nothing
//     i32.const 46
//     local.get 0
//     br_table 1 0 1 (default 0)
//   end
//   i32.const 100, i32.add, br 0
// end
// i32.add
function brTable(discard) {
    return {
        type: typeI32ToI32,
        code: branchOutOfBlock(discard, [
            0x02, i32, 0x41, 46, 0x20, 0x00, 0x0e, 0x03, 0x01, 0x00, 0x01, 0x00, 0x0b,
            0x41, 0xe4, 0x00, 0x6a, 0x0c, 0x00,
        ]),
    };
}

function instantiate(functions) {
    const module = new Bytes();
    module.push(0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00);

    const types = new Bytes();
    types.push(2, 0x60, 0, 1, i32, 0x60, 1, i32, 1, i32);
    section(module, 1, types);

    const funcs = new Bytes();
    funcs.push(...uleb(functions.length), ...functions.map(f => f.type));
    section(module, 3, funcs);

    const exports = new Bytes();
    exports.push(...uleb(functions.length));
    functions.forEach((f, index) => {
        const exportName = "f" + index;
        exports.push(...uleb(exportName.length), ...Array.from(exportName, c => c.charCodeAt(0)), 0x00, ...uleb(index));
    });
    section(module, 7, exports);

    const code = new Bytes();
    code.push(...uleb(functions.length));
    for (const f of functions) {
        const body = new Bytes();
        body.push(0); // no locals
        body.append(f.code);
        body.push(0x0b); // end
        code.push(...uleb(body.length));
        code.append(body);
    }
    section(module, 10, code);

    return new WebAssembly.Instance(new WebAssembly.Module(module.bytes())).exports;
}

const discards = [0, 3, 65535, 65536, 70000];

const simple = [];
for (const discard of discards)
    simple.push(br(discard), brIf(discard), brOnNull(discard));
const simpleExports = instantiate(simple);
simple.forEach((f, index) => {
    const result = simpleExports["f" + index]();
    if (result !== f.expected)
        throw new Error(`function ${index}: expected ${f.expected} but got ${result}`);
});

// Each br_table interleaves targets that discard nothing with ones that discard `discard` values.
const tableExports = instantiate(discards.map(brTable));
discards.forEach((discard, index) => {
    for (const [selector, expected] of [[0, 53], [1, 153], [2, 53], [3, 153], [100, 153]]) {
        const result = tableExports["f" + index](selector);
        if (result !== expected)
            throw new Error(`br_table discarding ${discard} with selector ${selector}: expected ${expected} but got ${result}`);
    }
});
