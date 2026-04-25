function insertNaNWhileFilling()
{
    var array = new Array(6);
    for (var i = 0; i < 4; ++i)
        array[i] = i;
    array[5] = NaN;
    return array;
}
noInline(insertNaNWhileFilling);

function testInsertNaNWhileFilling()
{
    var array = insertNaNWhileFilling();
    for (var i = 0; i < 4; ++i) {
        var value = array[i];
        if (value !== i) {
            throw "Failed testInsertNaNWhileFilling, value = " + value + " instead of " + i;
        }
    }
    var nan = array[5];
    if (!Number.isNaN(nan))
        throw "Failed testInsertNaNWhileFilling, array[5] is " + nan + " instead of NaN";
}
noInline(testInsertNaNWhileFilling);

for (var i = 0; i < 1e4; ++i) {
    testInsertNaNWhileFilling();
}


function insertNaNAfterFilling()
{
    var array = new Array(6);
    for (var i = 0; i < 5; ++i)
        array[i] = i;
    array[5] = NaN;
    return array;
}
noInline(insertNaNAfterFilling);

function testInsertNaNAfterFilling()
{
    var array = insertNaNAfterFilling();
    for (var i = 0; i < 4; ++i) {
        var value = array[i];
        if (value !== i) {
            throw "Failed testInsertNaNAfterFilling, value = " + value + " instead of " + i;
        }
    }
    var nan = array[5];
    if (!Number.isNaN(nan))
        throw "Failed testInsertNaNAfterFilling, array[5] is " + nan + " instead of NaN";
}
noInline(testInsertNaNAfterFilling);

for (var i = 0; i < 1e4; ++i) {
    testInsertNaNAfterFilling();
}


function pushNaNWhileFilling()
{
    var array = [];
    for (var i = 0; i < 5; ++i)
        array.push(i);
    array.push(NaN);
    return array;
}
noInline(pushNaNWhileFilling);

function testPushNaNWhileFilling()
{
    var array = pushNaNWhileFilling();
    for (var i = 0; i < 4; ++i) {
        var value = array[i];
        if (value !== i) {
            throw "Failed testPushNaNWhileFilling, value = " + value + " instead of " + i;
        }
    }
    var nan = array[5];
    if (!Number.isNaN(nan))
        throw "Failed testPushNaNWhileFilling, array[5] is " + nan + " instead of NaN";
}
noInline(testPushNaNWhileFilling);

for (var i = 0; i < 1e4; ++i) {
    testPushNaNWhileFilling();
}