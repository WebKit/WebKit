function test(array, target, start, end) {
    array.copyWithin(target, start, end);
}
noInline(test);

let array = new Array(1024);
for (let j = 0; j < array.length; j++)
    array[j] = j + 0.5;

for (let i = 0; i < 1e6; i++) {
    array.copyWithin(i % 512, 256, 768);
}
