//@ runDefault("--initialMarkingVersion=4294967294")

// Starts each heap two Full collections short of the marking version's wrap. The wrap collection resets every block to
// nullVersion, keeping the marks of blocks whose marks were current, as the survivors of the collection before. A block
// that collection does not reach has no live cells left, and allocation reuses it with those marks still in it; the next
// Full collection must not take them for survivors. fullGC() leaves sweeping to allocation, so the empty blocks stay.

function makeObject(i) {
    return { a0: i, a1: i, a2: i, a3: i, a4: i, a5: i, a6: i, a7: i, a8: i, a9: i,
        a10: i, a11: i, a12: i, a13: i, a14: i, a15: i, a16: i, a17: i, a18: i, a19: i };
}

let objects = [];
for (let i = 0; i < 10000; ++i)
    objects.push(makeObject(i));
fullGC(); // Marks these objects' blocks at the last version before the wrap.

objects = null;
fullGC(); // The wrap collection, which reaches none of those blocks.

let survivors = [];
for (let i = 0; i < 1000; ++i)
    survivors.push(makeObject(i));
fullGC(); // Reaches the new objects, in reused blocks that still hold the old marks.

for (let i = 0; i < survivors.length; ++i) {
    if (survivors[i].a19 !== i)
        throw new Error("Bad object at " + i);
}
