function foo() {
    return function bar(str) {
        var barBefore = bar;
        var result = eval(str);
        return [
            barBefore,
            bar,
            function () {
                return bar;
            },
            result
        ];
    }
}

function check() {
    var bar = foo();
    
    function verify(result, barAfter, evalResult) {
        if (result[0] !== bar)
            throw "Error: bad first entry: " + result[0];
        if (result[1] !== barAfter)
            throw "Error: bad first entry: " + result[1];
        var subResult = result[2]();
        if (subResult !== barAfter)
            throw "Error: bad second entry: " + result[2] + "; returned: " + subResult;
        if (result[3] !== evalResult)
            throw "Error: bad third entry: " + result[3] + "; expected: " + evalResult;
    }
    
    verify(bar("42"), bar, 42);
    verify(bar("bar"), bar, bar);
    verify(bar("var bar = 42; function fuzz() { return bar; }; fuzz()"), 42, 42);
}

// Execute check() more than once. At the time that we wrote this regression test, trunk would fail on
// the second execution. Executing 100 times would also gives us some optimizing JIT coverage.
for (var i = 0; i < 100; ++i)
    check();

