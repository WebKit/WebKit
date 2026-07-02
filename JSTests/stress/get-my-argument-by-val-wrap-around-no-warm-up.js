function foo(index) {
    return arguments[index];
}

noInline(foo);

var result = foo(-1);
if (result !== void 0)
    throw "Error: bad result at end: " + result;
