//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// The constants do not fit in int32, so the adds only stay in int32 by truncating them. The outer add
// can overflow int32 and is computed as Int52, which must not keep the inner add from being truncated.
function hash(seed) {
    seed = ((seed + 0xd3a2646c) ^ (seed << 9)) & 0xffffffff;
    seed = ((seed + 0x80000001) + (seed << 3)) & 0xffffffff;
    return seed;
}
noInline(hash);

function referenceHash(seed) {
    seed = Number(BigInt.asIntN(32, BigInt(seed) + 0xd3a2646cn)) ^ (seed << 9);
    seed = Number(BigInt.asIntN(32, BigInt(seed) + 0x80000001n + BigInt(seed << 3)));
    return seed;
}

var seeds = [];
var expected = [];
var seed = 49734321;
for (var i = 0; i < 1024; ++i) {
    seeds.push(seed);
    expected.push(referenceHash(seed));
    seed = (Math.imul(seed, 1103515245) + 12345) | 0;
}

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(hash(seeds[i & 1023]), expected[i & 1023]);

if (numberOfDFGCompiles(hash) > 1)
    throw new Error('hash should not keep exiting, but was compiled ' + numberOfDFGCompiles(hash) + ' times');
