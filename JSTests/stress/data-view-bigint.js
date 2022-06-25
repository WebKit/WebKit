function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 0x80, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]);
var dataView = new DataView(array.buffer);

shouldBe(dataView.getBigInt64(0), 0x01020304050607n);
shouldBe(dataView.getBigUint64(0), 0x01020304050607n);
shouldBe(dataView.getBigInt64(8), -9223088349902469625n);
shouldBe(dataView.getBigUint64(8), 9223655723807081991n);

shouldBe(dataView.setBigInt64(0, -1n), undefined);
shouldBe(dataView.getBigInt64(0), -1n);
shouldBe(dataView.getBigUint64(0), 0xffffffffffffffffn);

shouldBe(dataView.setBigUint64(0, 0xfffffffffffffffen), undefined);
shouldBe(dataView.getBigInt64(0), -2n);
shouldBe(dataView.getBigUint64(0), 0xfffffffffffffffen);

shouldBe(dataView.setBigUint64(0, 0x1fffffffffffffffen), undefined);
shouldBe(dataView.getBigInt64(0), -2n);
shouldBe(dataView.getBigUint64(0), 0xfffffffffffffffen);
shouldBe(dataView.getUint8(0), 0xff);
