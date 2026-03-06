function foo() {
    var array = [];
    for (var i = 0; i < 250000; i++)
        array.push(i);
    
    var result = 0;
    var iterator = array.entries();
    for (;;) {
        const { done, value: entry } = iterator.next();
        if (done) {
            break;
        }
        const [key, value] = entry;
        result += key + value + array[key];
    }

    return result;
}

var result = foo() + foo();
if (result != 187499250000)
    throw "Bad result: " + result;

