function foo() {
    var array = [];
    var result = [];
    for (var i = 0; i < 42; ++i)
        result.push(array.push(7 - i));
    return [array, result];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo();
    if (result[0].toString() != "7,6,5,4,3,2,1,0,-1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16,-17,-18,-19,-20,-21,-22,-23,-24,-25,-26,-27,-28,-29,-30,-31,-32,-33,-34")
        throw "Error: bad array: " + result[0];
    if (result[1].toString() != "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42")
        throw "Error: bad array: " + result[1];
}

