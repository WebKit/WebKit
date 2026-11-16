function test(typedArray) {
    return [...typedArray];
}
noInline(test);

const typedArray = new Uint8Array(32);
for (let i = 0; i < typedArray.length; i++)
    typedArray[i] = i;

for (let i = 0; i < 1e6; i++)
    test(typedArray);
