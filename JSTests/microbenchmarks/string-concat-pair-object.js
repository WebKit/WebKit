function foo(a) {
    return "foo" + new String(a);
}

var result;
for (var i = 0; i < 100000; ++i)
    result = foo("hello");

if (result != "foohello")
    throw "Error: bad result: " + result;
