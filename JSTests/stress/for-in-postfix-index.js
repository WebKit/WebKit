//@ runDefault

function foo(o) {
    var result = 0;
    for (var s in o) {
        var tmp = s++;
        result += o[s];
        result += tmp;
    }
    return result;
}

var result = foo({f:42});
if ("" + result != "NaN")
    throw "Error: bad result: " + result;
