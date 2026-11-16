function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

const count = 200;

const buffers = [];
const views = [];
for (let i = 0; i < count; i++) {
    const buffer = new ArrayBuffer(64);
    buffers.push(buffer);
    views.push(new Uint8Array(buffer));
}
edenGC();
for (let i = 0; i < count; i++) {
    views.push(new Int32Array(buffers[i], 4, 4));
    new DataView(buffers[i], 1, 2);
}
edenGC();
fullGC();
edenGC();
for (let i = 0; i < count; i++) {
    views.push(new Uint8Array(buffers[i], 1, 3));
    buffers[i].transfer();
}
for (const view of views)
    shouldBe(view.length, 0);

const orphans = [];
for (let i = 0; i < count; i++) {
    const view = new Uint8Array(new ArrayBuffer(32));
    view[1] = 7;
    orphans.push(view.subarray(1, 9));
}
edenGC();
for (let i = 0; i < count; i++)
    orphans[i] = orphans[i].subarray(0, 4);
edenGC();
fullGC();
for (let i = 0; i < count; i++) {
    shouldBe(orphans[i][0], 7);
    shouldBe(orphans[i].buffer.byteLength, 32);
}

const materialized = [];
for (let i = 0; i < count; i++) {
    const view = new Uint8Array(16);
    view[2] = 9;
    materialized.push(view);
}
edenGC();
for (let i = 0; i < count; i++)
    materialized[i] = new Uint8Array(materialized[i].buffer, 2, 4);
edenGC();
edenGC();
for (let i = 0; i < count; i++)
    shouldBe(materialized[i][0], 9);

for (let i = 0; i < count; i++) {
    const buffer = new ArrayBuffer(16);
    new Uint8Array(buffer);
    new Int8Array(buffer);
}
edenGC();

const stale = [];
for (let i = 0; i < count; i++) {
    const buffer = new ArrayBuffer(64);
    stale.push(buffer, new Uint8Array(buffer));
}
edenGC();
for (let round = 0; round < 3; round++) {
    for (let pass = 0; pass < 2; pass++) {
        for (let i = 0; i < count; i++) {
            for (let j = 0; j < 8; j++)
                new Uint8Array(stale[i * 2], j, 8);
        }
        edenGC();
    }
    const shared = new ArrayBuffer(64);
    const fresh = [];
    for (let i = 0; i < count * 16; i++)
        fresh.push(new Uint8Array(shared, 0, 8));
    for (let i = 0; i < count; i++) {
        const moved = stale[i * 2].transfer();
        shouldBe(stale[i * 2 + 1].length, 0);
        stale[i * 2] = moved;
        stale[i * 2 + 1] = new Uint8Array(moved);
    }
    for (const view of fresh)
        shouldBe(view.length, 8);
    edenGC();
}
