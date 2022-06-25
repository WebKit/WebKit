
var count = 0;
function test(object) {
    sum = 0;
    for (var i in object)
        sum += object[i];
    return sum;
}
noInline(test);


var array = new Array(100);
array.fill(1);
for (let i = 0; i < 1e5; ++i)
    test(array);
