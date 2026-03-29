function foo() {
    var array = [];
    for (var i = 0; i < 25000; i++)
        array.push(i);
    
    var result = 0;
    var iterator = array[Symbol.iterator]();
    for (;;) {
        const { done, value } = iterator.next();
        if (done) {
            break;
        }
        result += value;
    }

    return result;
}

var result = foo() + foo();
if (result != 624975000)
    throw "Bad result: " + result;