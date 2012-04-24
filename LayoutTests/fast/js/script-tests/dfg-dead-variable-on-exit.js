description(
"This tests that if a variable is dead on OSR exit, it will at least contain a valid JS value."
);

var array = [];

for (var i = 0; i < 9; ++i) {
    var code = "";
    code += "(function(";
    for (var j = 0; j < i; ++j) {
        if (j)
            code += ", ";
        code += "arg" + j;
    }
    code += ") {\n";
    code += "    return ";
    if (i) {
        for (var j = 0; j < i; ++j) {
            if (j)
                code += " + ";
            code += "arg" + j;
        }
    } else
        code += "void 0";
    code += ";\n";
    code += "})";
    array[i] = eval(code);
}

function foo(a, b) {
    var x = 0;
    if (a.f < b.f) {
        var result = b.g - a.g;
        x = !x;
        return result;
    } else {
        var result = a.g - b.g;
        x = [x];
        return result;
    }
}

var firstArg = {f:2, g:3};
var secondArg = {f:3, g:4};

for (var i = 0; i < 300; ++i) {
    var code = "";
    code += "array[" + (((i / 2) | 0) % array.length) + "](";
    for (var j = 0; j < (((i / 2) | 0) % array.length); ++j) {
        if (j)
            code += ", ";
        code += i + j;
    }
    if (i == 150) {
        firstArg = {f:2, g:2.5};
        secondArg = {f:3, g:3.5};
    }
    var tmp = firstArg;
    firstArg = secondArg;
    secondArg = tmp;
    code += "); foo(firstArg, secondArg)";
    shouldBe(code, "1");
}

