function test(array, target, start, end) {
    array.copyWithin(target, start, end);
}
noInline(test);

const array = new Array(1024);
for (let i = 0, l = array.length; i < l; ++i) {
    if (i % 3 !== 0) {
        array[i] = i;
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    test(array, i % 512, 256, 768);
}