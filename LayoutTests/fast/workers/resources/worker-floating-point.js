postMessage("Start test");

function runTest1() {
    var count = 0;
    var value = 1;
    while (value / 2) {
        value /= 2;
        ++count;
    }

    if (count == 1074)
        return "PASS: " + count;
    return "FAIL: expect 1074, actual " + count;
}

postMessage("Test 1: " + runTest1());

function doDiv(a, b)
{
    return a / b;
}

function runTest2() {
    count = doDiv(807930891584112.1, 1000000000.1);
    if (count == 807930.89150331901085)
        return "PASS: " + count;
    return "FAIL: expect 807930.89150331901085, actual " + count;
}

postMessage("Test 2: " + runTest2());

postMessage("DONE");
