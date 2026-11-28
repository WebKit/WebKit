function sum(array) {
    var result = 0;
    var iterator = array.values();
    while (true) {
        var step = iterator.next();
        if (step.done)
            break;
        result += step.value;
    }
    return result;
}
noInline(sum);

var array = [];
for (var i = 0; i < 1024; ++i)
    array.push(i);

for (var i = 0; i < 2e4; ++i) {
    if (sum(array) !== 523776)
        throw new Error("bad sum");
}
