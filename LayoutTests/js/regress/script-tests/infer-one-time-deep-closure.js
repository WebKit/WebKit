function fooMaker(aParam) {
    var a = aParam;
    return function(bParam) {
        var b = bParam;
        return function(cParam) {
            var c = cParam;
            return function(dParam) {
                var d = dParam;
                return function(eParam) {
                    var e = eParam;
                    return function (fParam) {
                        var f = a + b + c + d + e + fParam;
                        for (var i = 0; i < 1000; ++i)
                            f += a;
                        return f;
                    };
                };
            };
        };
    };
}

var foo = fooMaker(42)(1)(2)(3)(4);

noInline(foo);

for (var i = 0; i < 20000; ++i) {
    var result = foo(5);
    if (result != 42057)
        throw "Error: bad result: " + result;
}

var result = fooMaker(23)(2)(3)(4)(5)(5);
if (result != 23042)
    throw "Error: bad result: " + result;
