function test(array1, array2)
{
    return array1.concat(array2);
}
noInline(test);

var array0 = [];
var array1 = [0, 1, 2];
var array2 = ["String", 2];
for (var i = 0; i < 1e5; ++i) {
    test(array0, array0);
    test(array1, array1);
    test(array2, array2);
}
