function foo(a) {
    return "foo" + a + "bar";
}

var result;
for (var i = 0; i < 1000000; ++i)
    result = foo("hello");

if (result != "foohellobar")
    throw "Error: bad result: " + result;
