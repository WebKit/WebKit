//@ runDefault("--libpasForcePGMWithRate=1")

// An object with only named properties has no IndexingHeader in its butterfly, since that header
// only describes indexed elements. This test checks that the address returned by indexingHeader()
// in that case is never read, because it lies past the end of the butterfly's allocation, so
// reading it is invalid and can crash.
//
// Usually such a read would go unnoticed, because the memory after a butterfly is normally mapped.
// --libpasForcePGMWithRate=1 puts an inaccessible page after every allocation, forcing a crash.
//
// This is a regression test for webkit.org/b/324343 which crashed on builds done with GCC 13 or
// newer at -O2. It is worth running everywhere regardless, because the error was in our code
// rather than in the compiler, and newer Clang versions could be affected.
//
// It does not use testLoopCount, because what reproduces the bug is the shape of the heap, not
// how often a loop runs. 48 properties grow the storage through capacities 4, 8, 16, 32 and 64,
// reallocating at each step. The object count matters too: without the fix 54 objects never crash
// and 55 always do, so 150 leaves a wide margin and still runs in ten milliseconds.

const objectCount = 150;
const propertiesPerObject = 48;

const objects = [];
for (let objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
    const object = {};
    for (let propertyIndex = 0; propertyIndex < propertiesPerObject; ++propertyIndex)
        object["p" + propertyIndex] = objectIndex * 1000 + propertyIndex;
    objects.push(object);
}

for (let objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
    for (let propertyIndex = 0; propertyIndex < propertiesPerObject; ++propertyIndex) {
        const expected = objectIndex * 1000 + propertyIndex;
        const actual = objects[objectIndex]["p" + propertyIndex];
        if (actual !== expected)
            throw new Error(`object ${objectIndex} property p${propertyIndex}: expected ${expected}, got ${actual}`);
    }
}
