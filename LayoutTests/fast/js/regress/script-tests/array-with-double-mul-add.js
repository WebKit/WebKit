function foo() {
    var a = [];
    var b = [];
    var c = [];
    
    for (var i = 0; i < 1000; ++i) {
        a.push(i + 0.5);
        b.push(i - 0.5);
        c.push((i % 2) ? 0.5 : -0.25);
    }
    
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < a.length; ++j)
            a[j] += b[j] * c[j];
    }

    var result = 0;
    for (var i = 0; i < a.length; ++i)
        result += a[i];
    
    return result;
}

var result = foo();
if (result != 63062500)
    throw "Bad result: " + result;


