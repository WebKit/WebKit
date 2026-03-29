function foo() {
    var array = [];
    for (var i = 0; i < 25000; i++)
        array.push(i);
    
    var result = 0;
    var iterator = array.keys();
    for (;;) {
        const { done, value: key } = iterator.next();
        if (done) {
            break;
        }
        result += key + array[key];
    }

    return result;
}

var result = foo() + foo();
if (result != 1249950000)
    throw "Bad result: " + result;