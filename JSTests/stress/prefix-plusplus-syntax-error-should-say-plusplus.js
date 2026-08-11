//@ runDefault

str = "\n++x?.y;";

var exception;
try {
    eval(str);
} catch (e) {
    exception = e;
}

if (exception != "SyntaxError: Prefix ++ operator applied to value that is not a reference.")
    throw "FAILED";
