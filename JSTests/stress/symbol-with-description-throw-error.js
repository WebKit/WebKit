function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}
noInline(shouldThrow);

function test(description)
{
    return Symbol(description);
}
noInline(test);

var count = 0;
var flag = false;
var object = {
    toString()
    {
        count++;
        if (flag) {
            throw new Error("out");
        }
        return "Cocoa";
    }
};

for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(object).description, "Cocoa");
    shouldBe(count, i + 1);
}
flag = true;

shouldThrow(() => {
    shouldBe(test(object).description, "Cocoa");
    shouldBe(count, 1e4 + 1);
}, `Error: out`);
