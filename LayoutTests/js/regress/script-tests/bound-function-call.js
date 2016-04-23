function foo() {
    return this.f;
}

var binding = foo.bind({f:42});

(function() {
    var n = 1000000;
    var result = 0;
    for (var i = 0; i < n; ++i) {
        var myResult = binding();
        result += myResult;
    }
    if (result != n * 42)
        throw "Error: bad result: " + result;
})();
