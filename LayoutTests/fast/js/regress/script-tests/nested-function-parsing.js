(function() {
    var result = 0;

    for (var i = 0; i < 100; ++i) {
        var program = "(function f" + i + "() {\n";
        for (var j = 0; j < 1000; ++j) {
            program += "function f" + j + "() { return 0 && 1 && 2 && 3 && 4 && 5 && 6 && 7 && 8 && 9 && 10; }\n";
        }
        program += "})();\n";
        program += "return 0;\n";

        result += new Function(program)();
    }

    if (result != 0) {
        print("Bad result: " + result);
        throw "Error";
    }
})();
