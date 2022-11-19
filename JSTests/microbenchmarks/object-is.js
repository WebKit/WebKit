//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"

function incognito(value) {
    var array = [];
    array.push(value);
    array.push("ignore me");
    array.push(value);
    array.push({ ignore: "me" });
    return array[((Math.random() * 2) | 0) * 2];
}

// cached Object.is
var objectIs = Object.is;

// pure JS version of Object.is
function sameValue(a, b) {
    return (a === b) ?
        (a !== 0 || (1 / a === 1 / b)) :
        (a !== a && b !== b);
}

var testFiveA = incognito("back5");
var testFiveB = incognito("2back5".substring(1));
var testPi = incognito("PI");
var testNaN = incognito(NaN);
var testNaN_2 = incognito(NaN);

var result;

function test1()
{
    return testFiveA === testFiveB;
}
noInline(test1);

function test2()
{
    return Object.is(testFiveA, testFiveB);
}
noInline(test2);

function test3()
{
    return sameValue(testFiveA, testFiveB);
}
noInline(test3);

function test4()
{
    return testFiveA === testPi;
}
noInline(test4);

function test5()
{
    return Object.is(testFiveA, testPi);
}
noInline(test5);

function test6()
{
    return sameValue(testFiveA, testPi);
}
noInline(test6);

var verbose = false;
var tests = [
//     test1,
    test2,
//     test3,
//     test4,
    test5,
//     test6,
];
for (let test of tests) {
    if (verbose)
        var time = Date.now();

    for (let i = 0; i < 2e7; ++i)
        test();

    if (verbose)
        print(Date.now() - time);
}
