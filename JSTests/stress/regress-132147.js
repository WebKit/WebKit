var foo = function(a, b, count) {
    a = a | 0;
    b = b | 0;

    if (false) {
        return 1;
    } else {
        a = a & 0xff00;
        b = b & 0x00ff;

        orA = a | 0xff00;
        xorB = b ^ 0xff;
    }

    return orA | xorB;
};

var argA = 0;
var argB = 0x22;
var result = 0;

noInline(foo)

for (i = 0; i < 100000; i++)
    result = result | foo(argA, argB, 4)

if (result != 0xffdd)
    throw new Error("Incorrect result!");
