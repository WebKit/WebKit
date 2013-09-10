function foo() {
    var array = [];
    
    for (var i = 0; i < 1000; ++i)
        array.push(i + 0.5);
    
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < array.length; ++j)
            result += array[j];
    }
    
    return result;
}

if (foo() != 500000000)
    throw "ERROR";

