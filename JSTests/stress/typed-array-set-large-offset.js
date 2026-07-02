//@skip
// This tests takes >4s even in release mode on an M1 MBP, so I'd rather avoid running it on EWS by default.

let giga = 1024 * 1024 * 1024;
let sourceLength = giga;
let destinationLength = 4 * giga;
let offset = 3 * giga;

var source = new Uint8ClampedArray(sourceLength);
for (var i = 0; i < 100; ++i)
    source[i] = i & 0x3F;

var destination = new Int8Array(destinationLength);
destination.set(source, offset);

// Before the offset
let value = destination[42];
if (value !== 0)
    throw "At index " + 42 + ", expected 0 but got " + value;

// After the offset
for (var i = 0; i < 100; ++i) {
    let index = offset + i;
    let value = destination[index];
    let expectedValue = (index - offset) & 0x3F;
    if (value != expectedValue)
        throw "At index " + index + ", expected " + expectedValue + " but got " + value;
}
