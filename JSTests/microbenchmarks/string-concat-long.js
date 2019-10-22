//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(a, b, c, d, e)
{
    return a.concat(b, c, d, e);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test("Cocoa", "Cappuccino", "Matcha", "Rize", "Kilimanjaro");
