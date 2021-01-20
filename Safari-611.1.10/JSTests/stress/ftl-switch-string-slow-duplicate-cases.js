function foo(s) {
    switch (s) {
    case "ƑẦǏŁ":
    case "ÌŅ":
    case "ṤĻŐⱲ":
    case "ṔÄȚĦ":
        return 42;
    case "due":
    case "to":
    case "16-bit":
    case "strings":
        return 43;
    default:
        return 44;
    }
}

noInline(foo);

function cat(a, b) {
    return a + b;
}

for (var i = 0; i < 10000; ++i) {
    var result = foo(cat("16-", "bit"));
    if (result != 43)
        throw "Error: bad result (1): " + result;
    result = foo("ƑẦǏŁ");
    if (result != 42)
        throw "Error: bad result (2): " + result;
}
