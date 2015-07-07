function foo() {
    var array = [];
    
    for (var i = 0; i < 1000; ++i)
        array.push(i + ((i % 2) ? 0.5 : 0));
    
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < array.length; ++j)
            result += array[j];
    }
    
    return result;
}

var result = foo();
if (result != 499750000)
    throw "Bad result: " + result;

