//@ runDefault("--useWarmUpMarkedBlocks=1", "--warmUpMarkedBlockCount=64")
//@ runDefault("--useWarmUpMarkedBlocks=1", "--warmUpMarkedBlockCount=1")
//@ runDefault("--useWarmUpMarkedBlocks=1", "--warmUpMarkedBlockCount=0")
//@ runDefault("--useWarmUpMarkedBlocks=0")
//@ runDefault("--useWarmUpMarkedBlocks=1", "--warmUpMarkedBlockCount=8", "--scribbleFreeCells=1")
//@ runDefault("--useWarmUpMarkedBlocks=1", "--forceMiniVMMode=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

let chain = null;
let expectedLength = 0;
for (let i = 0; i < 300000; ++i) {
    const garbage = { index: i, payload: [i, i + 1, i + 2] };
    shouldBe(garbage.payload[2], i + 2);
    if (!(i % 1000)) {
        chain = { index: i, next: chain };
        ++expectedLength;
    }
}

let length = 0;
for (let node = chain; node; node = node.next) {
    ++length;
    shouldBe(node.index, (expectedLength - length) * 1000);
}
shouldBe(length, expectedLength);
