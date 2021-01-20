function exponentIsNonNanDouble1(x, doubleArrayIndex) {
    var doubleArray = [4.4];
    return Math.pow(x, doubleArray[doubleArrayIndex]);
}
noInline(exponentIsNonNanDouble1);

function exponentIsNonNanDouble2(x, doubleArray) {
    return Math.pow(x, doubleArray[0]);
}
noInline(exponentIsNonNanDouble2);

function testExponentIsDoubleConstant() {
    for (var i = 0; i < 10000; ++i) {
        var result = exponentIsNonNanDouble1(2, 0);
        if (result !== 21.112126572366314)
            throw "Error: exponentIsNonNanDouble1(2, 0) should be 21.112126572366314, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = exponentIsNonNanDouble2(3, [-1.5]);
        if (result !== 0.19245008972987526)
            throw "Error: exponentIsNonNanDouble2(3, [-1.5]) should be 0.19245008972987526, was = " + result;
    }
}
testExponentIsDoubleConstant();
