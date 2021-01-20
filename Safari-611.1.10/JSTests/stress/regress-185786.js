function foo() {
    return new Proxy({},
        new Proxy({}, {
            get: function () {
                throw "expected exception";
            }
        })
    ); 
}

var a = foo();
var b = Object.create(a);

var exception;
try {
    for (var v in b) { }
} catch (e) {
    exception = e;
}

if (exception != "expected exception")
    throw "FAIL";
