if (window.layoutTestController)
    layoutTestController.dumpAsText();

var console_messages = null;

function log(message)
{
    if (!console_messages) {
        console_messages = document.createElement("pre");
        document.body.appendChild(console_messages);
    }
    console_messages.appendChild(document.createTextNode(message + "\n"));
}

function fetchText(url) {
    try {
        var req = new XMLHttpRequest;
        req.open("GET", url, false);
        try {
            req.overrideMimeType("text/plain; charset=utf-8");
        } catch (ex) {
        }
        req.send();
        return req.responseText;
    } catch (ex) {
        log ("FAILED to read " + url);
    }
}

function replaceEscapes(str) {
    var pos;
    var endPos;
    
    // \x{41}
    while ((pos = str.indexOf("\\x{")) != -1) {
        endPos = pos + 1;
        while (str[endPos] != "}")
            ++endPos;
        var code = parseInt(str.substring(pos + 3, endPos), 16);
        str = str.substr(0, pos) + String.fromCharCode(code) + str.substr(endPos + 1);
    }

    // \071
    while ((pos = str.indexOf("\\0")) != -1) {
        endPos = pos + 4;
        var code = parseInt(str.substring(pos + 2, endPos), 8);
        str = str.substr(0, pos) + String.fromCharCode(code) + str.substr(endPos);
    }

    // \x7f
    while ((pos = str.indexOf("\\x")) != -1) {
        endPos = pos + 4;
        var code = parseInt(str.substring(pos + 2, endPos), 16);
        str = str.substr(0, pos) + String.fromCharCode(code) + str.substr(endPos);
    }

    return str;
}

function removeTrailingSpaces(str) {
    return str.replace(/ *$/, '');
}

function runPCRETest(number, name) {
    log(name + "\n");

    var inputText = fetchText("testinput" + number);
    var outputText = fetchText("testoutput" + number);
    
    var tests = inputText.split(/\n[ \t]*\n/);
    var results = outputText.split(/\n[ \t]*\n/);
    if (tests.length != results.length)
        log("Number of tests in input doesn't match output");
    
    var i, j;
    for (i = 0 ; i < tests.length - 1; ++i) { // -1  to account for "End of ..."
        var testLines = tests[i].split("\n");
        var resultLines = results[i].split("\n");

        var linesInRegexp = 1;
        var regexpText = testLines[0];
        if (testLines[0].lastIndexOf(testLines[0][0]) == 0) {
            do {
                regexpText += "\n" + testLines[linesInRegexp];
            } while (testLines[linesInRegexp++].indexOf(regexpText[0]) == -1)
        }

        log(regexpText);

        regexpText = replaceEscapes(regexpText);
        for (n = linesInRegexp; n < testLines.length; ++n) {
            // Some tests have trailing whitespace that needs to be ignored - but not if it is encoded.
            testLines[n] = replaceEscapes(removeTrailingSpaces(testLines[n]));
        }
        for (n = linesInRegexp; n < resultLines.length; ++n) {
            resultLines[n] = replaceEscapes(resultLines[n]);
        }

        var regexp = null;
        try {
            var reText = regexpText.match(new RegExp("^" + testLines[0][0] + "(.*)" + testLines[0][0] + ".*$"))[1];
            var reFlags = regexpText.match(new RegExp("^" + testLines[0][0] + ".*" + testLines[0][0] + "([^ \t]*)[ \t]*$"))[1];
            reFlags = reFlags.replace("8", ""); // No UTF-8 mode, we always work with Unicode.
            if (!/^[gims]*$/.test(reFlags)) {
                // Allowing "s" to better test newline handling, although it's unsupported in JavaScript.
                log("Unsupported modifiers: "+ reFlags + "\n");
                continue;
            }
            regexp = new RegExp(reText, reFlags);
        } catch (ex) {
        }
        if (!regexp) {
            log("FAILED TO COMPILE\n");
            continue;
        }

        var resultsRow = linesInRegexp;
        for (j = linesInRegexp; j < testLines.length; ++j) {
            var testLine = testLines[j];
            var failersLine = (/\*\*\* Failers/.test(testLine));

            // There is a slight discrepancy in the ways input and output strings are decoded, so the test below has
            // to be less specific than it could be. No big deal, as it's just a sanity check for debugging.
            if (removeTrailingSpaces(resultLines[resultsRow]) != removeTrailingSpaces(testLine))
                log('Test results out of sync, "' + resultLines[resultsRow] + '" vs. "' + testLine + '".');

            ++resultsRow;
            while (/^ {0,1}[0-9]+: .*/.test(resultLines[resultsRow]) || /^No match$/.test(resultLines[resultsRow])) {

                var actualResults = regexp.exec(testLine.replace(/^    /, ""));
                var expectedResults = null;
                while (/^ {0,1}[0-9]+: .*/.test(resultLines[resultsRow])) {
                    var startOfNewResultArray = /^ {0,1}0: .*/.test(resultLines[resultsRow]);
                    if (!expectedResults)
                        expectedResults = [];
                    else if (startOfNewResultArray)
                        break;
                    var resultLineWithoutNumber = resultLines[resultsRow++].replace(/^ {0,1}[0-9]*: /, "");
                    if (resultLineWithoutNumber == "<unset>")
                        resultLineWithoutNumber = undefined;
                    expectedResults[expectedResults.length] = resultLineWithoutNumber;
                }
                if (/^No match$/.test(resultLines[resultsRow]))
                    ++resultsRow;
                if ((actualResults === null && expectedResults === null) || (actualResults && expectedResults && actualResults.toString() == expectedResults.toString()))
                    log(testLine + (failersLine ? "" : ": PASS"));
                else
                    log(testLine + ': FAIL. Actual results: "' + actualResults /*+ '"; expected: "' + expectedResults*/ + '"');
            }
        }
        log("");
    }
    log("DONE");
}
