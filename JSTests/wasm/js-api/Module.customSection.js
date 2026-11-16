import Builder from '../Builder.js';
import * as assert from '../assert.js';

assert.throws(() => WebAssembly.Module.customSections(undefined, ""), TypeError, `WebAssembly.Module.customSections called with non WebAssembly.Module argument`);
assert.eq(WebAssembly.Module.customSections.length, 2);

{
    const empty = new WebAssembly.Module((new Builder()).WebAssembly().get());
    assert.isArray(WebAssembly.Module.customSections(empty, ""));
    assert.eq(WebAssembly.Module.customSections(empty, "").length, 0);
}

{
    const single = new WebAssembly.Module((new Builder())
        .Unknown("hello").Byte(0x00).Byte(0x42).Byte(0xFF).End()
        .WebAssembly().get());
    assert.eq(WebAssembly.Module.customSections(single, "").length, 0);
    const hello = WebAssembly.Module.customSections(single, "hello");
    assert.eq(hello.length, 1);
    assert.eq(hello[0].byteLength, 3);
    const helloI8 = new Int8Array(hello[0]);
    assert.eq(helloI8[0], 0x00);
    assert.eq(helloI8[1], 0x42);
    assert.eq(helloI8[2], -1);
}

{
    const unicode = new WebAssembly.Module((new Builder())
        .Unknown("👨‍❤️‍💋‍👨").Byte(42).End()
        .WebAssembly().get());
    const family = WebAssembly.Module.customSections(unicode, "👨‍❤️‍💋‍👨");
    assert.eq(family.length, 1);
    assert.eq(family[0].byteLength, 1);
    const familyI8 = new Int8Array(family[0]);
    assert.eq(familyI8[0], 42);
}

{
    const many = new WebAssembly.Module((new Builder())
        .Unknown("zero").Byte(0).End()
        .Unknown("one").Byte(1).Byte(1).End()
        .Unknown("one").Byte(2).Byte(2).Byte(2).End()
        .Unknown("two").Byte(3).Byte(3).Byte(3).Byte(3).End()
        .Unknown("one").Byte(4).Byte(4).Byte(4).Byte(4).Byte(4).End()
        .WebAssembly().get());

    const zero = WebAssembly.Module.customSections(many, "zero");
    assert.eq(zero.length, 1);
    assert.eq(zero[0].byteLength, 1);
    const zeroI8 = new Int8Array(zero[0]);
    assert.eq(zeroI8[0], 0);

    const two = WebAssembly.Module.customSections(many, "two");
    assert.eq(two.length, 1);
    assert.eq(two[0].byteLength, 4);
    const twoI8 = new Int8Array(two[0]);
    assert.eq(twoI8[0], 3);
    assert.eq(twoI8[1], 3);
    assert.eq(twoI8[2], 3);
    assert.eq(twoI8[3], 3);

    const one = WebAssembly.Module.customSections(many, "one");
    assert.eq(one.length, 3);
    let seen = 0;
    const expect = [
        [1, 1],
        [2, 2, 2],
        [4, 4, 4, 4, 4],
    ];
    for (const section of one) {
        assert.eq(section.byteLength, expect[seen].length);
        const I8 = new Int8Array(section);
        for (let i = 0; i < expect[seen].length; ++i)
            assert.eq(I8[i], expect[seen][i]);
        ++seen;
    }
}

{
    const module = new WebAssembly.Module((new Builder())
        .Unknown("copy").Byte(0x11).Byte(0x22).Byte(0x33).End()
        .WebAssembly().get());

    const first = WebAssembly.Module.customSections(module, "copy");
    assert.eq(first.length, 1);
    const second = WebAssembly.Module.customSections(module, "copy");
    assert.eq(second.length, 1);

    assert.eq(first[0] === second[0], false);
    assert.eq(first === second, false);

    const firstBytes = new Uint8Array(first[0]);
    firstBytes[0] = 0xFF;
    firstBytes[1] = 0xEE;
    firstBytes[2] = 0xDD;

    const again = WebAssembly.Module.customSections(module, "copy");
    assert.eq(again.length, 1);
    assert.eq(again[0] === first[0], false);
    const againBytes = new Uint8Array(again[0]);
    assert.eq(againBytes[0], 0x11);
    assert.eq(againBytes[1], 0x22);
    assert.eq(againBytes[2], 0x33);

    const secondBytes = new Uint8Array(second[0]);
    assert.eq(secondBytes[0], 0x11);
    assert.eq(secondBytes[1], 0x22);
    assert.eq(secondBytes[2], 0x33);
}
