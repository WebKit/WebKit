// Example of the serialized data.
// var array = new Uint8Array([
//     7, 0, 0, 0,
//     47, // BigInt
//     1, // signed
//     2, 0, 0, 0, // length
//     0, 0, 0, 0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0,
// ]);
const BigIntTag = 47;

let serialized = new Uint8Array(internals.serializeObject(0xffffffffffffffffffffffffffffffffn));
shouldBe(`serialized.length`, `26`);
shouldBe(`serialized[4]`, `BigIntTag`);
// Clear payload with zero.
let offset = 4 + 1 + 1 + 4;
for (let i = 0; i < 16; ++i) {
    serialized[offset + i] = 0;
}
let result = internals.deserializeBuffer(serialized.buffer);
shouldBe(`result`, `0n`);
shouldBeTrue(`result == 0`);


serialized = new Uint8Array(internals.serializeObject(0xffffffffn));
shouldBe(`serialized.length`, `18`);
shouldBe(`serialized[4]`, `BigIntTag`);
// Clear payload with zero.
for (let i = 0; i < 8; ++i) {
    serialized[offset + i] = 0;
}
result = internals.deserializeBuffer(serialized.buffer);
shouldBe(`result`, `0n`);
shouldBeTrue(`result == 0`);
