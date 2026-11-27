//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ defaultNoEagerRun

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual + " expected: " + expected);
}

class Slot {
    constructor(offset) { this._offset = offset; }
    get offset() { return this._offset; }
}

function readOffset(slot) {
    return slot.offset;
}
noInline(readOffset);

function maxOfNegatedOffsets(slots) {
    let result = 0;
    for (let slot of slots)
        result = Math.max(result, -slot.offset);
    return result;
}
noInline(maxOfNegatedOffsets);

function makeSlots(kind) {
    let slots = [];
    for (let i = 0; i < 16; ++i) {
        let offset;
        switch (kind) {
        case 0:
            offset = -8 * (i + 1);
            break;
        case 1:
            offset = i == 7 ? undefined : -8 * (i + 1);
            break;
        case 2:
            offset = Math.ceil((8 * (i + 1)) / 3) * -1.5;
            break;
        case 3:
            offset = i == 3 ? null : -8 * (i + 1);
            break;
        }
        slots.push(new Slot(offset));
    }
    return slots;
}

let inputs = [makeSlots(0), makeSlots(1), makeSlots(2), makeSlots(3)];
let expected = inputs.map((slots) => slots.reduce((result, slot) => Math.max(result, -slot._offset), 0));
shouldBe(expected[0], 128);
shouldBe(expected[1], NaN);
shouldBe(expected[2], 64.5);
shouldBe(expected[3], 128);

for (let i = 0; i < testLoopCount * 4; ++i) {
    readOffset(inputs[0][i & 15]);
    readOffset(inputs[1][7]);
}

for (let i = 0; i < testLoopCount; ++i) {
    let kind = i < testLoopCount / 2 ? 0 : (i % 4);
    shouldBe(maxOfNegatedOffsets(inputs[kind]), expected[kind]);
}

function maxOfNegatedElements(values, undefinedIndex) {
    let result = 0;
    for (let i = 0; i < values.length; ++i) {
        let value = values[i];
        if (i === undefinedIndex)
            value = undefined;
        result = Math.max(result, -value);
    }
    return result;
}
noInline(maxOfNegatedElements);
noOSRExitFuzzing(maxOfNegatedElements);

let negativeInts = [];
for (let i = 0; i < 16; ++i)
    negativeInts.push(-8 * (i + 1));
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(maxOfNegatedElements(negativeInts, -1), 128);
if (numberOfDFGCompiles(maxOfNegatedElements) > 1)
    throw new Error("maxOfNegatedElements was compiled " + numberOfDFGCompiles(maxOfNegatedElements) + " times");
shouldBe(maxOfNegatedElements(negativeInts, 3), NaN);
