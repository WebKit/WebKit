function ropify(a,b,c) {
    return a + b + c;
}
noInline(ropify);

function ropify2(a,b,c) {
    return a + b + c;
}
noInline(ropify2);

let test = new String("test");

for (let i = 0; i < 100000; i++) {
    if (ropify("a", "b", test) !== "abtest")
        throw "wrong on warmup";
}

String.prototype[Symbol.toPrimitive] = function() { return "changed"; }

if (ropify("a", "b", test) !== "abchanged")
    throw "watchpoint didn't fire";


// Test we don't start doing the wrong thing if the prototype chain has been mucked with.
for (let i = 0; i < 100000; i++) {
    if (ropify2("a", "b", test) !== "abchanged")
        throw "wrong on warmup";
}
