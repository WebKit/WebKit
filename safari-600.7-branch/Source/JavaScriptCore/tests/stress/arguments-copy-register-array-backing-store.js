var foo = function(o) {
    return arguments;
};

var bar = function() {
    var a = Array.prototype.slice.call(arguments);
    var sum = 0;
    for (var i = 0; i < a.length; ++i)
        sum += a[i];
    return sum;
};

var args = foo({}, 1, 2, 3);
var expectedArgs = Array.prototype.slice.call(args);

edenGC();

var expectedResult = 0;
var result = 0;
for (var i = 0; i < 10000; ++i) {
    expectedResult += i + i + 1 + i + 2;
    result += bar(i, i + 1, i + 2);
}

if (result != expectedResult)
    throw new Error("Incorrect result: " + result + " != " + expectedResult);

for (var i = 0; i < expectedArgs.length; ++i) {
    if (args[i] !== expectedArgs[i])
        throw new Error("Incorrect arg result");
}
    
