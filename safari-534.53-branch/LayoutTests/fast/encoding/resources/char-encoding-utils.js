function encode(charset, unicode)
{
    // Returns a value already encoded, since we can't do it synchronously.
    return results[charset][unicode];
}

function testsDone()
{
    var form = document.getElementById('form');
    var subframe = document.getElementById('subframe');

    form.parentNode.removeChild(form);
    subframe.parentNode.removeChild(subframe);

    description("This tests encoding characters in various character sets.");

    for (i = 0; i < charsets.length; ++i) {
        shouldBe("encode('" + charsets[i] + "', '" + unicodes[i] + "')", "'" + expectedResults[i] + "'");
    }

    isSuccessfullyParsed();

    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

function processResult(result)
{
    var charsetResults = results[charsets[i]];
    if (!charsetResults) {
        charsetResults = new Object;
        results[charsets[i]] = charsetResults;
    }
    charsetResults[unicodes[i]] = result; 
}

function subframeLoaded()
{
    var URL = "" + document.getElementById('subframe').contentWindow.location;
    processResult(URL.substr(URL.indexOf('=') + 1));
    ++i;
    runTest();
}

function runTest()
{    
    if (i >= charsets.length) {
        testsDone();
        return;
    }

    var form = document.getElementById('form');
    var text = document.getElementById('text');
    var subframe = document.getElementById('subframe');

    form.acceptCharset = charsets[i];
    form.action = "resources/dummy.html";
    subframe.onload = subframeLoaded;
    text.value = String.fromCharCode(unicodes[i].replace('U+', '0x'));
    
    form.submit();
}

function testEncode(charsetName, unicode, characterSequence)
{
    charsets.push(charsetName);
    unicodes.push(unicode);
    expectedResults.push(characterSequence);
}
