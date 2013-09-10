function foo(a) {
    return new String(a);
}

var result;
for (var i = 0; i < 1000000; ++i)
    result = foo("hello");

if (result != "hello")
    throw new "Error: bad result: " + result;
