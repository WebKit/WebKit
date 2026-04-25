function test(array, searchElement) {
    return array.lastIndexOf(searchElement);
}
noInline(test);

const array = new Array(1024);
for (let i = 0; i < array.length; i++) {
    array[i] = i;
}

for (let i = 0; i < 1e5; i++) {
    test(array, 512);
    test(array, 256);
}
