const obj = {a: 0, b: 1, foo: 2, c: 3, d: 4};
const descNonWritable = {writable: false};
const descNonEnumerable = {enumerable: false};
const descWritable = {writable: true};
const descEnumerable = {enumerable: true};

for (let i = 0; i < 1e4; ++i) {
    Object.defineProperty(obj, "foo", descNonWritable);
    Object.defineProperty(obj, "foo", descNonEnumerable);
    Object.defineProperty(obj, "foo", descWritable);
    Object.defineProperty(obj, "foo", descEnumerable);
    Object.defineProperty(obj, "foo", {value: i});
}
