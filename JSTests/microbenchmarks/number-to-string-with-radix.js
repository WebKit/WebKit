//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test()
{
    for (var i = 0; i < 10; ++i) {
        var result = '';
        result += i.toString(2);
        result += i.toString(4);
        result += i.toString(8);
        result += i.toString(16);
        result += i.toString(32);
    }
    return result;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
