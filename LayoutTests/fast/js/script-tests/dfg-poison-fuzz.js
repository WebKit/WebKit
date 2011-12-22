description(
"This tests that DFG OSR exit's variable poisoning handles mixes of float and int variables correctly."
);

for (var __i = 0; __i < (1 << 11); ++__i) {
    code  = "function callee() {\n";
    code += "    var result = 0;\n";
    code += "    for (var i = 0; i < arguments.length; ++i)\n";
    code += "        result += arguments[i];\n";
    code += "    return result;\n";
    code += "}\n";
    code += "\n";
    code += "function registerBomb(a, b, c, d, e, f, g, h, i, j, k, l) {\n";
    code += "    return callee((a + b), (b + c), (c - a), (c - b), (a - c), (k - i), (j + h), (g - f), (f - e), (d - k), l, (j - i), (h - g), (f - e + d), (a - b + c), (k - a), (l + a));\n";
    code += "}\n";
    code += "\n";
    code += "var accumulator = 0;\n";
    code += "for (var ____i = 0; ____i < 100; ++____i)\n";
    code += "    accumulator += registerBomb(";
    for (var __j = 0; __j < 11; ++__j) {
        code += __j + 1;
        if ((1 << __j) & __i)
            code += ".5";
        code += ", ";
    }
    code += "12.5);\n";
    code += "var o = {dummy:accumulator, result:registerBomb(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, \"12.5\")};";
    code += "o";
    
    var o = eval(code);
    debug("Dummy value for __i = " + __i + " is: " + o.dummy);
    shouldBe("o.result", "\"2412.511521012.51\"");
}

