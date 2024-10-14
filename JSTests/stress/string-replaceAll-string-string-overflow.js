var exception;
try {
    var x = [];
    x[399] = "11";
    Array(1024).join(x).replaceAll(1, Array(1024).join(x.join("345")));
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
