//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
const chars = 'abcdefghijklmnopqrstuvwxyz';
var prim = '';
for (var i = 0; i < 32768; i++) {
    prim += chars.charAt(~~(Math.random() * 26));
}
const obj = new String(prim);

function test(obj)
{
    return obj.toString();
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(obj);
