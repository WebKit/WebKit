function arrayifyInt32(array)
{
    for (var i = 0; i < 1e2; ++i)
        array[i] = 42;
}
noInline(arrayifyInt32);

function arrayifyDouble(array)
{
    for (var i = 0; i < 1e2; ++i)
        array[i] = 42.195;
}
noInline(arrayifyDouble);

function arrayifyContiguous(array)
{
    for (var i = 0; i < 1e2; ++i)
        array[i] = true;
}
noInline(arrayifyContiguous);

for (var i = 0; i < 1e4; ++i) {
    let cocoa = { name: 'Cocoa' };
    let cappuccino = { name: 'Cappuccino' };
    arrayifyInt32(cocoa);
    arrayifyInt32(cappuccino);
}

for (var i = 0; i < 1e4; ++i) {
    let cocoa = { name: 'Cocoa' };
    let cappuccino = { name: 'Cappuccino' };
    arrayifyDouble(cocoa);
    arrayifyDouble(cappuccino);
}

for (var i = 0; i < 1e4; ++i) {
    let cocoa = { name: 'Cocoa' };
    let cappuccino = { name: 'Cappuccino' };
    arrayifyContiguous(cocoa);
    arrayifyContiguous(cappuccino);
}
