//@ skip if $memoryLimited and ["arm", "mips"].include?($architecture)
const obj = {};
for (let i = 0; i < 100; ++i)
    obj["k" + i] = i;

const descNonWritable = {writable: false};
const descNonEnumerable = {enumerable: false};
const descWritable = {writable: true};
const descEnumerable = {enumerable: true};

for (let i = 0; i < 1e4; ++i) {
    const key = "k" + (i % 100);
    Object.defineProperty(obj, key, descNonWritable);
    Object.defineProperty(obj, key, descNonEnumerable);
    Object.defineProperty(obj, key, descWritable);
    Object.defineProperty(obj, key, descEnumerable);
    Object.defineProperty(obj, key, {value: i});
}
