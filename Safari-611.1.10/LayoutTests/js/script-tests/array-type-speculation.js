description(
"This tests that storing into something that is not array does not crash."
);

theCode = "\n\
function storeFooByValOrDoArithmetic(o, p1, p2, v) {\n\
    var x;\n\
    if (p1) {\n\
        x = o.foo;\n\
    } else {\n\
        x = v;\n\
        if (p2) {\n\
            x--;\n\
        } else {\n\
            x++;\n\
        }\n\
    }\n\
    x[5] = \"foo\";\n\
}\n\
\n\
function runTheTest(p1, p2) {\n\
    var o = new Object();\n\
    o.foo = new Object();\n\
    storeFooByValOrDoArithmetic(o, p1, p2, 1);\n\
    return o.foo[5];\n\
}\n";

function runWithPredicates(predicateArray) {
    var myCode = theCode;
    
    for (var i = 0; i < predicateArray.length; ++i) {
        myCode += "result = runTheTest(" + predicateArray[i][0] + ", " + predicateArray[i][1] + ");\n";
        myCode += "shouldBe(\"result\", " + predicateArray[i][2] + ");\n";
    }
    
    eval(myCode);
}

runWithPredicates([[true, true, "\"\\\"foo\\\"\""], [true, false, "\"\\\"foo\\\"\""], [false, true, "\"undefined\""], [false, false, "\"undefined\""]]);
runWithPredicates([[false, false, "\"undefined\""], [true, false, "\"\\\"foo\\\"\""], [false, true, "\"undefined\""], [true, true, "\"\\\"foo\\\"\""]]);
runWithPredicates([[true, true, "\"\\\"foo\\\"\""], [false, true, "\"undefined\""], [true, false, "\"\\\"foo\\\"\""], [false, false, "\"undefined\""]]);
runWithPredicates([[false, false, "\"undefined\""], [false, true, "\"undefined\""], [true, false, "\"\\\"foo\\\"\""], [true, true, "\"\\\"foo\\\"\""]]);
