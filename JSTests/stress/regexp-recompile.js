function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var flag = false;
function recompile(regExp) {
    if (flag)
        regExp.compile("e");
}
noInline(recompile);

function target() {
    var regExp = /test/;
    recompile(regExp);
    regExp.lastIndex = 0;
    return regExp.exec("Hey");
}
noInline(target);

for (var i = 0; i < 1e4; ++i)
    shouldBe(target(), null);
flag = true;
shouldBe(JSON.stringify(target()), `["e"]`);
for (var i = 0; i < 1e4; ++i)
    shouldBe(JSON.stringify(target()), `["e"]`);
