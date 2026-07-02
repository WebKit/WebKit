description("Test immediate constants where objects are expects.  Should not crash.");

tests = [];
function createTest(expr) {
    tests.push(new Function(expr.replace("%value%", "true")));
    tests.push(new Function(expr.replace("%value%", "(-0)")));
    tests.push(new Function(expr.replace("%value%", "(0)")));
    tests.push(new Function(expr.replace("%value%", "(1)")));
    tests.push(new Function(expr.replace("%value%", "null")));
    tests.push(new Function(expr.replace("%value%", "undefined")));
}
num=1;
createTest("%value% instanceof Object");
createTest("Object instanceof %value%");
createTest("%value%.toString");
createTest("'toString' in %value%");
createTest("%value% in Object");
createTest("num << %value%");
createTest("%value% << num");
createTest("num >> %value%");
createTest("%value% >> num");
createTest("num >>> %value%");
createTest("%value% >>> num");
createTest("num ^ %value%");
createTest("%value% ^ num");
createTest("num | %value%");
createTest("%value% | num");
createTest("num & %value%");
createTest("%value% & num");
createTest("num + %value%");
createTest("%value% + num");
createTest("num - %value%");
createTest("%value% - num");
createTest("num * %value%");
createTest("%value% * num");
createTest("num / %value%");
createTest("%value% / num");
createTest("num % %value%");
createTest("%value% % num");
createTest("num || %value%");
createTest("%value% || num");
createTest("num && %value%");
createTest("%value% && num");
createTest("%value%()");
createTest("%value%.toString()");
createTest("Object[%value%]()");
createTest("for(var i in %value%) {  }");
createTest("var o = {a:1, b:2, c:3}; for(var i in o) { o = %value%; o[i]; }");

for (var i = 0; i < tests.length; i++) {
    try { tests[i](); } catch(e) {}
}
