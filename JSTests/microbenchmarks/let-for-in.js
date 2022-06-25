//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(o) {
    var count = 0;
    for (let p in o) {
        if (o[p])
            count++;
    }
    return count;
}
noInline(foo);

function Bar() {
    this.line = 1;
    this.column = 2;
    this.stack = "str";
}

var total = 0;
for (let j = 0; j < 100000; ++j)
    total += foo(new Bar);
if (total != 300000)
    throw new Error("Bad result: " + total);
