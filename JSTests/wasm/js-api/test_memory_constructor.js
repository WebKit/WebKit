// FIXME: use the assert library: https://bugs.webkit.org/show_bug.cgi?id=165684
//@ skip if $memoryLimited
import Builder from '../Builder.js';

function assert(b) {
    if (!b) {
        throw new Error("Bad assertion");
    }
}

{
    let threw = false;
    try {
        new WebAssembly.Memory({initial: 20, maximum: 19});
    } catch(e) {
        assert(e instanceof RangeError);
        assert(e.message === "'maximum' page count must be than greater than or equal to the 'initial' page count");
        threw = true;
    }
    assert(threw);
}

const pageSize = 64 * 1024;
const maxPageCount = (2**32) / pageSize;

function testInvalidSize(description, propName) {
    let threw = false;
    try {
        new WebAssembly.Memory(description);
    } catch(e) {
        threw = true;
        assert(e instanceof RangeError);
        assert(e.message === `WebAssembly.Memory '${propName}' page count is too large`);
    }
    assert(threw);
}

{
    function testInvalidInitial(v) {
        testInvalidSize({initial: v}, "initial");
    }

    try {
        new WebAssembly.Memory({initial: maxPageCount});
        new WebAssembly.Memory({initial: maxPageCount, maximum: maxPageCount});
    } catch(e) {
        // These might throw, since we're asking for a lot of memory.
    }

    testInvalidInitial(2**31);
    testInvalidInitial(maxPageCount + 1);
}

{
    function testInvalidMaximum(v) {
        testInvalidSize({initial: 1, maximum: v}, "maximum");
    }

    testInvalidMaximum(2**31);
    testInvalidMaximum(maxPageCount + 1);
}

{
    for (let i = 0; i < 5; i++) {
        let x = Math.random() * (2**10);
        x |= 0;
        const mem = new WebAssembly.Memory({initial: x, maximum: x + 100});
        assert(mem.buffer.byteLength === x * pageSize);
    }
}

{
    let bufferGetter = Object.getOwnPropertyDescriptor((new WebAssembly.Memory({initial:1})).__proto__, "buffer").get;
    let threw = false;
    try {
        bufferGetter.call({});
    } catch(e) {
        assert(e instanceof TypeError);
        assert(e.message === "WebAssembly.Memory.prototype.buffer getter called with non WebAssembly.Memory |this| value");
        threw = true;
    }
    assert(threw);
}

{
    const args = {minimum: 5};
    let minimum = false;
    const proxy = new Proxy(args, {
        get(target, prop, receiver) {
            if (prop === "minimum") {
                minimum = true;
            }
            return Reflect.get(...arguments);
        }
    })
    const mem = new WebAssembly.Memory(proxy);
    assert(mem.buffer.byteLength === 5 * pageSize);
    assert(minimum);

    let threw = false;
    try {
        new WebAssembly.Memory({minimum: 5, initial: 5});
    } catch (e) {
        assert(e instanceof TypeError);
        assert(e.message === "WebAssembly.Memory 'initial' and 'minimum' options are specified at the same time");
        threw = true;
    }
    assert(threw);
}
