function foo() {
    var array = [];
    for (var i = 0; i < 25000; i++)
        array.push(i);
    
    var result = 0;
    for (var [key, value] of array.entries())
        result += key + value + array[key];
    
    return result;
}

var result = foo() + foo();
if (result != 1874925000)
    throw "Bad result: " + result;

