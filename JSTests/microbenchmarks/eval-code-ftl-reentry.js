for (var _i = 0; _i < 1000; ++_i) {
    eval(
        "var result = 0;\n" +
        "var n = 15000;\n" + 
        "for (var i = 0; i < n; ++i)\n" +
        "    result += {f: 1}.f;\n" +
        "if (result != n)\n" +
        "    throw \"Error: bad result: \" + result;\n");
}

