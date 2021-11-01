//@skip
// This tests takes >4s even in release mode on an M1 MBP, so I'd rather avoid running it on EWS by default.

let giga = 1024 * 1024 * 1024;
let sourceLength = 2 * giga;
let destinationLength = 3 * giga;
let offset = giga;

var source = new Uint8ClampedArray(sourceLength);
for (var i = 0; i < 100; ++i)
    source[i] = i & 0x3F;
for (var i = 0; i < 100; ++i) {
    let index = giga + i;
    source[index] = index & 0x3F
}

var destination = new Int8Array(destinationLength);
destination.set(source, offset);

// Before the offset
let value = destination[42];
if (value !== 0)
    throw "At index " + 42 + ", expected 0 but got " + value;

// After the offset but before INT32_MAX
for (var i = 0; i < 100; ++i) {
    let index = offset + i;
    let value = destination[index];
    let expectedValue = (index - offset) & 0x3F;
    if (value != expectedValue)
        throw "At index " + index + ", expected " + expectedValue + " but got " + value;
}

// After the offset and greater than INT32_MAX
for (var i = 0; i < 100; ++i) {
    let index = offset + giga + i;
    let value = destination[index];
    let expectedValue = (index - offset) & 0x3F;
    if (value != expectedValue)
        throw "At index " + index + ", expected " + expectedValue + " but got " + value;
}
