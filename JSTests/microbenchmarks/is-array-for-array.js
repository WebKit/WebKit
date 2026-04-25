//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function isArray(array)
{
    return Array.isArray(array);
}
noInline(isArray);

for (var i = 0; i < 1e5; ++i)
    isArray([]);
