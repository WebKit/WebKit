// This tests that we can correctly infer that a local variable is only
// used as an integer even if its uses are not in the same basic block as
// where it is defined.

function foo(a, b, c) {
    var x = 1 + a + b;
    if (c)
        x++;
    else
        x--;
    return (x + a)|0;
}

var bigNumber = 2147483647;
var result = 0;

for (var i = 0; i < 10000000; ++i)
    result = (result + foo(i, bigNumber - i, i%2)) | 0;

if (result != -2014260032) {
    print("Got a bad result: " + result);
    throw "Bad result";
}


