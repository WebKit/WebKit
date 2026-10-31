function test(iterable) {
    return [...iterable];
}
noInline(test);

const object = {};
for (let i = 0; i < 64; i++)
    object["key" + i] = i;
Object.keys(object);
const keys = Object.keys(object);

for (let i = 0; i < 1e6; i++) {
    test(keys);
    test("a");
}
