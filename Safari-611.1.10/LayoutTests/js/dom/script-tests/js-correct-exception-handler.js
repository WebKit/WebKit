description("Verify that handle exceptions flowing throw inline callframes correctly");

function throwEventually() {
    if (value++ > 10000)
        throw new Error;
    return 5;
}

var value = 0;
function f(x) {
    var result = 0;
    function g(a) {
        return a * throwEventually();
    }
    for (var i = 0; i < 3; i++)
        i += i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * i * g(x);
    return i;
}

function test() {
    while (true)
        (function() {f(5)})()
}

shouldThrow("test()");

