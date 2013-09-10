function foo(a) {
    return "foo" + a;
}

var result;
for (var i = 0; i < 1000000; ++i)
    result = foo("hello");

if (result != "foohello")
    throw "Error: bad result: " + result;
