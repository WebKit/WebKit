description("Regression test for https://webkit.org/b/142625. This test should not crash.");

for (var i = 0; i < 10000; i++)
    eval("function fuzz() {}");

testPassed("Eval of DFG compiled function didn't crash");
