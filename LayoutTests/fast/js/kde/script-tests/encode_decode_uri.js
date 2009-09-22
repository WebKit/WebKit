// --------------------------------------------------------------------------------

var resolution = 251; // set to 1 for 100% coverage

function checkEncodeException(encodeFunctionName,c1,c2)
{
    if (c2 == undefined)
        shouldThrow(encodeFunctionName
            + "(String.fromCharCode(" + c1 + "))");
    else
        shouldThrow(encodeFunctionName
            + "(String.fromCharCode(" + c1 + ") + String.fromCharCode(" + c2 + "))");
}

function checkEncodeDecode(encodeFunctionName, decodeFunctionName, c1, c2)
{
    if (c2 == undefined)
        shouldBe(decodeFunctionName + "(" + encodeFunctionName
            + "(String.fromCharCode(" + c1 + ")))",
            "String.fromCharCode(" + c1 + ")");
    else
        shouldBe(decodeFunctionName + "(" + encodeFunctionName
            + "(String.fromCharCode(" + c1 + ") + String.fromCharCode(" + c2 + ")))",
            "String.fromCharCode(" + c1 + ") + String.fromCharCode(" + c2 + ")");
}

function checkWithFunctions(encodeFunction, decodeFunction)
{
    checkEncodeDecode(encodeFunction, decodeFunction, 0);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xD7FF);

    checkEncodeDecode(encodeFunction, decodeFunction, 0xE000);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xFFFD);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xFFFE);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xFFFF);

    checkEncodeException(encodeFunction, 0xDC00);
    checkEncodeException(encodeFunction, 0xDFFF);

    checkEncodeDecode(encodeFunction, decodeFunction, 0xD800, 0xDC00);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xDBFF, 0xDC00);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xD800, 0xDFFF);
    checkEncodeDecode(encodeFunction, decodeFunction, 0xDBFF, 0xDFFF);

    checkEncodeException(encodeFunction, 0xD800, 0);
    checkEncodeException(encodeFunction, 0xD800, 0xD7FF);
    checkEncodeException(encodeFunction, 0xD800, 0xD800);
    checkEncodeException(encodeFunction, 0xD800, 0xDBFF);
    checkEncodeException(encodeFunction, 0xD800, 0xE000);
    checkEncodeException(encodeFunction, 0xD800, 0xE000);
    checkEncodeException(encodeFunction, 0xD800, 0xFFFD);
    checkEncodeException(encodeFunction, 0xD800, 0xFFFE);
    checkEncodeException(encodeFunction, 0xD800, 0xFFFF);

    for (var charcode = 1; charcode < 0xD7FF; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, charcode);

    for (var charcode = 0xE001; charcode < 0xFFFD; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, charcode);

    for (var charcode = 0xDC01; charcode < 0xDFFF; charcode += resolution)
        checkEncodeException(encodeFunction, charcode);

    for (var charcode = 0xD801; charcode < 0xDBFF; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, charcode, 0xDC00);

    for (var charcode = 0xDC01; charcode < 0xDFFF; charcode += resolution)
        checkEncodeDecode(encodeFunction, decodeFunction, 0xD800, charcode);

    for (var charcode = 1; charcode < 0xDBFF; charcode += resolution)
        checkEncodeException(encodeFunction, 0xD800, charcode);

    for (var charcode = 0xE001; charcode < 0xFFFD; charcode += resolution)
        checkEncodeException(encodeFunction, 0xD800, charcode);
}

checkWithFunctions("encodeURI", "decodeURI");
checkWithFunctions("encodeURIComponent", "decodeURIComponent");

successfullyParsed = true;
