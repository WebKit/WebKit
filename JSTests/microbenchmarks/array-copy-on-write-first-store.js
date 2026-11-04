function test(object, value) {
    const keys = Object.keys(object);
    keys[0] = value;
    return keys;
}
noInline(test);

const object = {};
for (let i = 0; i < 64; i++)
    object["key" + i] = i;
Object.keys(object);

for (let i = 0; i < 1e6; i++) {
    const result = test(object, "changed");
    if (result.length !== 64 || result[0] !== "changed" || result[63] !== "key63")
        throw new Error("bad result");
}
