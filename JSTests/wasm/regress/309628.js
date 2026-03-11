"use strict";

function leb(n) {
    const out = [];
    do {
        let b = n & 0x7f;
        n >>>= 7;
        if (n) b |= 0x80;
        out.push(b);
    } while (n);
    return out;
}

/*
 * (module
 *   (func (export "f")
 *     (local $0 i64) ;; numLocals times
 *     (drop (local.get 0))))
 */
function makeModule(numLocals) {
    const body = [
        0x01, ...leb(numLocals), 0x7e,
        0x20, 0x00,
        0x1a,
        0x0b,
    ];
    const code = [0x01, ...leb(body.length), ...body];
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
        0x0a, ...leb(code.length), ...code,
    ]);
}

const workerSrc = `
${leb.toString()}
${makeModule.toString()}

for (let L = 35000; L >= 8000; L -= 2000) {
    const bytes = makeModule(L);
    const inst = new WebAssembly.Instance(new WebAssembly.Module(bytes));
    try {
        inst.exports.f();
        break;
    } catch (e) {
    }
}
`;

$.agent.start(workerSrc);
$.agent.sleep(2000);
