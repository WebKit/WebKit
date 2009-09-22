description(
'Tests whether eval() works inside statements that read and modify a value.'
);

function multTest()
{
    var x = 1;
    x *= eval('2');
    return x == 2;
}

function divTest()
{
    var x = 2;
    x /= eval('2');
    return x == 1;
}

function addTest()
{
    var x = 0;
    x += eval('1');
    return x == 1;
}

function subTest()
{
    var x = 0;
    x -= eval('1');
    return x == -1;
}

function lshiftTest()
{
    var x = 1;
    x <<= eval('1');
    return x == 2;
}

function rshiftTest()
{
    var x = 1;
    x >>= eval('1');
    return x == 0;
}

function urshiftTest()
{
    var x = 1;
    x >>>= eval('1');
    return x == 0;
}

function andTest()
{
    var x = 1;
    x &= eval('1');
    return x == 1;
}

function xorTest()
{
    var x = 0;
    x ^= eval('1');
    return x == 1;
}

function orTest()
{
    var x = 0;
    x |= eval('1');
    return x == 1;
}

function modTest()
{
    var x = 4;
    x %= eval('3');
    return x == 1;
}

function preIncTest()
{
    var x = { value: 0 };
    ++eval('x').value;
    return x.value == 1;
}

function preDecTest()
{
    var x = { value: 0 };
    --eval('x').value;
    return x.value == -1;
}

function postIncTest()
{
    var x = { value: 0 };
    eval('x').value++;
    return x.value == 1;
}

function postDecTest()
{
    var x = { value: 0 };
    eval('x').value--;
    return x.value == -1;
}

shouldBeTrue('multTest();');
shouldBeTrue('divTest();');
shouldBeTrue('addTest();');
shouldBeTrue('subTest();');
shouldBeTrue('lshiftTest();');
shouldBeTrue('rshiftTest();');
shouldBeTrue('urshiftTest();');
shouldBeTrue('andTest();');
shouldBeTrue('xorTest();');
shouldBeTrue('orTest();');
shouldBeTrue('modTest();');

shouldBeTrue('preIncTest();');
shouldBeTrue('preDecTest();');
shouldBeTrue('postIncTest();');
shouldBeTrue('postDecTest();');

successfullyParsed = true;
