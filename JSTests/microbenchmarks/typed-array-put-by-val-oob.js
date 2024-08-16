function test1(array, value) {
    array[value] = 42;
}
noInline(test1);

function test2(array, value) {
    "use strict";
    array[value] = 42;
}
noInline(test2);

var array = new Int32Array([1, 2, 3, 4, 5]);

for (var i = 0; i < 1e5; ++i) {
    test1(array, i & 0b111);
    test2(array, i & 0b111);
}
