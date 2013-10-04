function foo() {
    var array = [];
    for (var i = 0; i < 25000; i++)
        array.push(i);
    
    var result = 0;
    for (var i of array)
        result += i;
    
    return result;
}

var result = foo() + foo();
if (result != 624975000)
    throw "Bad result: " + result;

