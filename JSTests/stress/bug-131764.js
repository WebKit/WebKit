var result = 0;
function test1(arr) {
    return Array.of(...arr);
}
function test2() {
    return Array(...arguments);
}

var result = 0;
if (this.noInline) {
    noInline(test1)
    noInline(test2)
}

var array = [1,2,3,4,5];

for (var i = 0; i < 10000; i++) {
     result ^= test2(1,2,3,4,5,6,7).length;
}

if (result != 0)
    throw "Error: bad result: " + result;
