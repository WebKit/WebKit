function foo(o) {
    var count = 0;
    for (let p in o) {
        if (o[p])
            count++;
    }
    return count;
}
noInline(foo);

var total = 0;
for (let j = 0; j < 100000; ++j)
    total += foo(new Error);
if (total != 300000)
    throw new Error("Bad result: " + total);
