import Builder from '../Builder.js';
import * as assert from '../assert.js';

assert.throws(() => WebAssembly.Module.prototype.customSections(undefined, ""), TypeError, `WebAssembly.Module.prototype.customSections called with non WebAssembly.Module |this| value`);

{
    const empty = new WebAssembly.Module((new Builder()).WebAssembly().get());
    assert.isArray(empty.customSections(""));
    assert.eq(empty.customSections("").length, 0);
}

{
    const single = new WebAssembly.Module((new Builder())
        .Unknown("hello").Byte(0x00).Byte(0x42).Byte(0xFF).End()
        .WebAssembly().get());
    assert.eq(single.customSections("").length, 0);
    const hello = single.customSections("hello");
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
    const family = unicode.customSections("👨‍❤️‍💋‍👨");
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

    const zero = many.customSections("zero");
    assert.eq(zero.length, 1);
    assert.eq(zero[0].byteLength, 1);
    const zeroI8 = new Int8Array(zero[0]);
    assert.eq(zeroI8[0], 0);

    const two = many.customSections("two");
    assert.eq(two.length, 1);
    assert.eq(two[0].byteLength, 4);
    const twoI8 = new Int8Array(two[0]);
    assert.eq(twoI8[0], 3);
    assert.eq(twoI8[1], 3);
    assert.eq(twoI8[2], 3);
    assert.eq(twoI8[3], 3);

    const one = many.customSections("one");
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
