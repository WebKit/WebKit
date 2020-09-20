//@ skip unless $jitTests

function assert(a, e) {
    if (a !== e)
        throw new Error("Expected to be: " + e + " but got: " + a);
}

function foo(a) {
    return ~a;
}
noInline(foo);

let c = 0;
let o = {
    valueOf: () => {
        c++;
        return 3;
    }
};

for (let i = 0; i < 10000; i++)
    foo(o);

assert(c, 10000);
if (numberOfDFGCompiles(foo) > 1)
    throw new Error("Function 'foo' should be compiled just once");

