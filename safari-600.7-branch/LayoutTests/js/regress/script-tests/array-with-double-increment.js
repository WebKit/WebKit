function foo() {
    var array = [];
    
    for (var i = 0; i < 1000; ++i)
        array.push(i + 0.5);
    
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < array.length; ++j)
            array[j]++;
    }

    var result = 0;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    
    return result;
}

var result = foo();
if (result != 1500000)
    throw "Bad result: " + result;


