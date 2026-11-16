//@ runDefault("--useConcurrentGC=0")
//@ runDefault("--useConcurrentGC=0", "--useDFGJIT=0")

// An old Contiguous array has to be remembered when unshift stores a new cell into it.

function unshift(array, value) {
    return array.unshift(value);
}
noInline(unshift);

function makeArray() {
    const array = [{}, {}, {}, {}];
    array.pop();
    return array;
}

for (let i = 0; i < testLoopCount; ++i)
    unshift(makeArray(), {});

const arrays = [];
for (let i = 0; i < 500; ++i)
    arrays.push(makeArray());
fullGC();

for (let i = 0; i < arrays.length; ++i)
    unshift(arrays[i], { index: i, nested: [i, i + 1] });
edenGC();

const garbage = [];
for (let i = 0; i < 20000; ++i)
    garbage.push({ index: -1, nested: [-1, -1] });

for (let i = 0; i < arrays.length; ++i) {
    const value = arrays[i][0];
    if (value.index !== i || value.nested[1] !== i + 1)
        throw new Error("Bad value at " + i);
}
