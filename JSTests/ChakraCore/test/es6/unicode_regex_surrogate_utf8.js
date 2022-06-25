//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.defineProperty(Object.getPrototypeOf({}), "echo", { value: function () { WScript.Echo(this); } });
Object.defineProperty(Object.getPrototypeOf({}), "echos", { value: function () { WScript.Echo(JSON.stringify(this)); } });
Object.defineProperty(Object.getPrototypeOf({}), "echoChars", {
    value: function () {
        var chars = [];
        var that = String(this);
        for (var i = 0; i < that.length; i++) {
            chars.push(that.charCodeAt(i));
        }
        chars.echos();
    }
});

Object.defineProperty(Object.getPrototypeOf({}), "toHex", { value: function () { 
    var num = Number(this);
    var str = "";
    for(; num > 0; num = Math.floor(num / 16)) {
        var digit = num % 16
        var d = digit < 10 ? String(digit) : 
            (digit === 10 ? "A" : 
            (digit === 11 ? "B" :
            (digit === 12 ? "C" :
            (digit === 13 ? "D" :
            (digit === 14 ? "E" : "F")))));
        str = d + str;
    }
    return str;
}});

Object.defineProperty(Object.getPrototypeOf({}), "getCP", {
    value: function () {
        var chars = [];
        var that = String(this);
        for (var i = 0; i < that.length;) {
            var cp = that.codePointAt(i);
            chars.push(cp.toHex());
            i += cp > 0x10000 ? 2 : 1;
        }
        return chars;
    }
});

Object.defineProperty(Object.getPrototypeOf({}), "echoCP", { value: function () { this.getCP().echos(); } });



function testRegexExec(reg, str, expected, msg) {
    if(reg.unicode)
    {
        ("Testing with unicode flag.").echo();
    }
    var result = reg.exec(str);

    if (JSON.stringify(result[0]) !== JSON.stringify(expected)) {
        ("Failed: " + msg).echo();
        ("Regex: " + reg.toString()).echo();
        ("Result in CodePoint Values: " + result[0].getCP()).echo();
        ("Expected in CodePoint Values: " + expected.getCP()).echo();
    } else {
        ("Passed: " + msg).echo();
    }
}

if ('\ud801\udc27'.match(/[\u0000-\ud801\udc4f]/u) === null) WScript.Echo('Expected \ud801\udc27 to be matched by \ud801\udc4f');
if ('\uD842\uDFB7'.match(/./u)[0] === 2) WScript.Echo('Expected \uD842\uDFB7 to be matched by \'.\' if unicode flag is passed');

var numFailures = 0;
var totalTests = 0;
function testCaseEquivalence(first, second, type) {
    totalTests += 1;
    var failed = [];

    if (String(new RegExp(first, "ui").exec(second)) !== (second)) {
        failed.push(1);
    } 
    if (String(new RegExp(second, "ui").exec(first)) !== (first)) {
        failed.push(2);
    } 
    if (String(new RegExp("[\u0000" + second + "\uDBFF\uDFFF]", "ui").exec(first)) !== (first)) {
        failed.push(3);
    } 
    if (String(new RegExp("[\u0000" + first + "\uDBFF\uDFFF]", "ui").exec(second)) !== (second)) {
        failed.push(4);
    } 
    if (String(eval("/" + first + "/ui.exec(second)")) !== (second)) {
        failed.push(5);
    } 
    if (String(eval("/" + second + "/ui.exec(first)")) !== (first)) {
        failed.push(6);
    } 
    if (String(eval("/[\u0000" + second + "\uDBFF\uDFFF]/ui.exec(first)")) !== (first)) {
        failed.push(7);
    } 
    if (String(eval("/[\u0000" + first + "\uDBFF\uDFFF]/ui.exec(second)")) !== (second)) {
        failed.push(8);
    }
    if (String(new RegExp("[\u0000a-z" + second + "\uDBFF\uDFFF]", "ui").exec(first)) !== (first)) {
        failed.push(9);
    } 
    if (String(new RegExp("[\u0000a-z" + first + "\uDBFF\uDFFF]", "ui").exec(second)) !== (second)) {
        failed.push(10);
    } 
    if (String(eval("/[\u0000a-z" + second + "\uDBFF\uDFFF]/ui.exec(first)")) !== (first)) {
        failed.push(11);
    } 
    if (String(eval("/[\u0000a-z" + first + "\uDBFF\uDFFF]/ui.exec(second)")) !== (second)) {
        failed.push(12);
    }

    if (failed.length > 0) {
        numFailures += 1;
        ("Type: " + type + " - Case equivalence failed for: " + first.getCP() + ", and " + second.getCP() + "\t Subtests that failed: " + JSON.stringify(failed)).echo();
    }
}


 //Disjunctions of the form (x|y|z) are compiled to a set node [xyz] for normal characters, and remain as disjunctions for surrogate pairs.
 //Test both case sensitive/insensitive
"============ 1. Regex tests without unicode option ============".echo();

testRegexExec(/(a|b)/, "ab", "a", "Basic dijunction of two chars.");
testRegexExec(/(a|b)*/, "ab", "ab", "Basic dijunction of two chars.");
testRegexExec(/(a|b)*/i, "Ab", "Ab", "Case insensitive dijunction of two chars.");
testRegexExec(/(a|b|[c-z])*/i, "AbxY", "AbxY", "Case insensitive dijunction of two chars + range.");
testRegexExec(/(a|b|[c-y])*/i, "AbxYz", "AbxY", "Case insensitive dijunction of two chars + range.");
testRegexExec(/(a|b|[d-y])*/i, "AbxcYz", "Abx", "Case insensitive dijunction of two chars + range.");
testRegexExec(/(\u0061|\142|[\u{0063}-\x7A])*/ui, "AbxYz", "AbxYz", "Case insensitive dijunction of two chars + range with escape sequence.");
testRegexExec(/(\u0061|\142|[\u{0063}-\x79])*/ui, "AbxYz", "AbxY", "Case insensitive dijunction of two chars + range with escape sequence.");
testRegexExec(/(\u0061|\142|[\u{0065}-\x79])*/ui, "AbxDYz", "Abx", "Case insensitive dijunction of two chars + range with escape sequence.");

"== Negation Testing ==".echo();
testRegexExec(/[^a-z]+/, "ab d", " ", "Simple negation testing.");
testRegexExec(/[^a-z]+/, "ab \tde\r\n", " \t", "Simple negation testing.");
testRegexExec(/[^a-z]+/, "aB \tde\r\n", "B \t", "Simple negation testing.");
testRegexExec(/[^a-z\uDC00]+/, "ab \t\u{10000}de\r\n", " \t\uD800", "Simple negation testing. Partial surrogate");
testRegexExec(/[^a-z]+/i, "aB \tDe\r\n", " \t", "Case insensitive negation testing.");
testRegexExec(/[^a-z\uDC00]+/i, "aB\uDC00 \t\u{10000}De\r\n", " \t\uD800", "Case insensitive negation testing. Partial surrogate");

"============ 2. Same tests as 1, but with unicode option ============".echo();

testRegexExec(/(a|b)/u, "ab", "a", "Basic dijunction of two chars.");
testRegexExec(/(a|b)*/u, "ab", "ab", "Basic dijunction of two chars.");
testRegexExec(/(a|b)*/iu, "Ab", "Ab", "Case insensitive dijunction of two chars.");
testRegexExec(/(a|b|[c-z])*/ui, "AbxY", "AbxY", "Case insensitive dijunction of two chars + range.");
testRegexExec(/(a|b|[c-y])*/ui, "AbxYz", "AbxY", "Case insensitive dijunction of two chars + range.");
testRegexExec(/(a|b|[d-y])*/ui, "AbxcYz", "Abx", "Case insensitive dijunction of two chars + range.");
testRegexExec(/(\u0061|\142|[\u{0063}-\x7A])*/ui, "AbxYz", "AbxYz", "Case insensitive dijunction of two chars + range with escape sequence.");
testRegexExec(/(\u0061|\142|[\u{0063}-\x79])*/ui, "AbxYz", "AbxY", "Case insensitive dijunction of two chars + range with escape sequence.");
testRegexExec(/(\u0061|\142|[\u{0065}-\x79])*/ui, "AbxDYz", "Abx", "Case insensitive dijunction of two chars + range with escape sequence.");

"== Negation Testing ==".echo();
testRegexExec(/[^a-z]+/u, "ab d", " ", "Simple negation testing.");
testRegexExec(/[^a-z]+/u, "ab \tde\r\n", " \t", "Simple negation testing.");
testRegexExec(/[^a-z]+/u, "aB \tde\r\n", "B \t", "Simple negation testing.");
testRegexExec(/[^a-z\uDC00]+/u, "ab \t\u{10000}de\r\n", " \t\uD800\uDC00", "Simple negation testing. Partial surrogate");
testRegexExec(/[^a-z]+/ui, "aB \tDe\r\n", " \t", "Case insensitive negation testing.");

"============ 3. Supplementary chars tests ============".echo();


testCaseEquivalence("\u{0041}", "\u{0061}", "C");//C
testCaseEquivalence("\u{0042}", "\u{0062}", "C");//C
testCaseEquivalence("\u{0043}", "\u{0063}", "C");//C
testCaseEquivalence("\u{0044}", "\u{0064}", "C");//C
testCaseEquivalence("\u{0045}", "\u{0065}", "C");//C
testCaseEquivalence("\u{0046}", "\u{0066}", "C");//C
testCaseEquivalence("\u{0047}", "\u{0067}", "C");//C
testCaseEquivalence("\u{0048}", "\u{0068}", "C");//C
testCaseEquivalence("\u{0049}", "\u{0069}", "C");//C
testCaseEquivalence("\u{0049}", "\u{0131}", "T");//T
testCaseEquivalence("\u{004A}", "\u{006A}", "C");//C
testCaseEquivalence("\u{004B}", "\u{006B}", "C");//C
testCaseEquivalence("\u{004C}", "\u{006C}", "C");//C
testCaseEquivalence("\u{004D}", "\u{006D}", "C");//C
testCaseEquivalence("\u{004E}", "\u{006E}", "C");//C
testCaseEquivalence("\u{004F}", "\u{006F}", "C");//C
testCaseEquivalence("\u{0050}", "\u{0070}", "C");//C
testCaseEquivalence("\u{0051}", "\u{0071}", "C");//C
testCaseEquivalence("\u{0052}", "\u{0072}", "C");//C
testCaseEquivalence("\u{0053}", "\u{0073}", "C");//C
testCaseEquivalence("\u{0054}", "\u{0074}", "C");//C
testCaseEquivalence("\u{0055}", "\u{0075}", "C");//C
testCaseEquivalence("\u{0056}", "\u{0076}", "C");//C
testCaseEquivalence("\u{0057}", "\u{0077}", "C");//C
testCaseEquivalence("\u{0058}", "\u{0078}", "C");//C
testCaseEquivalence("\u{0059}", "\u{0079}", "C");//C
testCaseEquivalence("\u{005A}", "\u{007A}", "C");//C
testCaseEquivalence("\u{00B5}", "\u{03BC}", "C");//C
testCaseEquivalence("\u{00C0}", "\u{00E0}", "C");//C
testCaseEquivalence("\u{00C1}", "\u{00E1}", "C");//C
testCaseEquivalence("\u{00C2}", "\u{00E2}", "C");//C
testCaseEquivalence("\u{00C3}", "\u{00E3}", "C");//C
testCaseEquivalence("\u{00C4}", "\u{00E4}", "C");//C
testCaseEquivalence("\u{00C5}", "\u{00E5}", "C");//C
testCaseEquivalence("\u{00C6}", "\u{00E6}", "C");//C
testCaseEquivalence("\u{00C7}", "\u{00E7}", "C");//C
testCaseEquivalence("\u{00C8}", "\u{00E8}", "C");//C
testCaseEquivalence("\u{00C9}", "\u{00E9}", "C");//C
testCaseEquivalence("\u{00CA}", "\u{00EA}", "C");//C
testCaseEquivalence("\u{00CB}", "\u{00EB}", "C");//C
testCaseEquivalence("\u{00CC}", "\u{00EC}", "C");//C
testCaseEquivalence("\u{00CD}", "\u{00ED}", "C");//C
testCaseEquivalence("\u{00CE}", "\u{00EE}", "C");//C
testCaseEquivalence("\u{00CF}", "\u{00EF}", "C");//C
testCaseEquivalence("\u{00D0}", "\u{00F0}", "C");//C
testCaseEquivalence("\u{00D1}", "\u{00F1}", "C");//C
testCaseEquivalence("\u{00D2}", "\u{00F2}", "C");//C
testCaseEquivalence("\u{00D3}", "\u{00F3}", "C");//C
testCaseEquivalence("\u{00D4}", "\u{00F4}", "C");//C
testCaseEquivalence("\u{00D5}", "\u{00F5}", "C");//C
testCaseEquivalence("\u{00D6}", "\u{00F6}", "C");//C
testCaseEquivalence("\u{00D8}", "\u{00F8}", "C");//C
testCaseEquivalence("\u{00D9}", "\u{00F9}", "C");//C
testCaseEquivalence("\u{00DA}", "\u{00FA}", "C");//C
testCaseEquivalence("\u{00DB}", "\u{00FB}", "C");//C
testCaseEquivalence("\u{00DC}", "\u{00FC}", "C");//C
testCaseEquivalence("\u{00DD}", "\u{00FD}", "C");//C
testCaseEquivalence("\u{00DE}", "\u{00FE}", "C");//C
testCaseEquivalence("\u{0100}", "\u{0101}", "C");//C
testCaseEquivalence("\u{0102}", "\u{0103}", "C");//C
testCaseEquivalence("\u{0104}", "\u{0105}", "C");//C
testCaseEquivalence("\u{0106}", "\u{0107}", "C");//C
testCaseEquivalence("\u{0108}", "\u{0109}", "C");//C
testCaseEquivalence("\u{010A}", "\u{010B}", "C");//C
testCaseEquivalence("\u{010C}", "\u{010D}", "C");//C
testCaseEquivalence("\u{010E}", "\u{010F}", "C");//C
testCaseEquivalence("\u{0110}", "\u{0111}", "C");//C
testCaseEquivalence("\u{0112}", "\u{0113}", "C");//C
testCaseEquivalence("\u{0114}", "\u{0115}", "C");//C
testCaseEquivalence("\u{0116}", "\u{0117}", "C");//C
testCaseEquivalence("\u{0118}", "\u{0119}", "C");//C
testCaseEquivalence("\u{011A}", "\u{011B}", "C");//C
testCaseEquivalence("\u{011C}", "\u{011D}", "C");//C
testCaseEquivalence("\u{011E}", "\u{011F}", "C");//C
testCaseEquivalence("\u{0120}", "\u{0121}", "C");//C
testCaseEquivalence("\u{0122}", "\u{0123}", "C");//C
testCaseEquivalence("\u{0124}", "\u{0125}", "C");//C
testCaseEquivalence("\u{0126}", "\u{0127}", "C");//C
testCaseEquivalence("\u{0128}", "\u{0129}", "C");//C
testCaseEquivalence("\u{012A}", "\u{012B}", "C");//C
testCaseEquivalence("\u{012C}", "\u{012D}", "C");//C
testCaseEquivalence("\u{012E}", "\u{012F}", "C");//C
testCaseEquivalence("\u{0130}", "\u{0069}", "T");//T
testCaseEquivalence("\u{0132}", "\u{0133}", "C");//C
testCaseEquivalence("\u{0134}", "\u{0135}", "C");//C
testCaseEquivalence("\u{0136}", "\u{0137}", "C");//C
testCaseEquivalence("\u{0139}", "\u{013A}", "C");//C
testCaseEquivalence("\u{013B}", "\u{013C}", "C");//C
testCaseEquivalence("\u{013D}", "\u{013E}", "C");//C
testCaseEquivalence("\u{013F}", "\u{0140}", "C");//C
testCaseEquivalence("\u{0141}", "\u{0142}", "C");//C
testCaseEquivalence("\u{0143}", "\u{0144}", "C");//C
testCaseEquivalence("\u{0145}", "\u{0146}", "C");//C
testCaseEquivalence("\u{0147}", "\u{0148}", "C");//C
testCaseEquivalence("\u{014A}", "\u{014B}", "C");//C
testCaseEquivalence("\u{014C}", "\u{014D}", "C");//C
testCaseEquivalence("\u{014E}", "\u{014F}", "C");//C
testCaseEquivalence("\u{0150}", "\u{0151}", "C");//C
testCaseEquivalence("\u{0152}", "\u{0153}", "C");//C
testCaseEquivalence("\u{0154}", "\u{0155}", "C");//C
testCaseEquivalence("\u{0156}", "\u{0157}", "C");//C
testCaseEquivalence("\u{0158}", "\u{0159}", "C");//C
testCaseEquivalence("\u{015A}", "\u{015B}", "C");//C
testCaseEquivalence("\u{015C}", "\u{015D}", "C");//C
testCaseEquivalence("\u{015E}", "\u{015F}", "C");//C
testCaseEquivalence("\u{0160}", "\u{0161}", "C");//C
testCaseEquivalence("\u{0162}", "\u{0163}", "C");//C
testCaseEquivalence("\u{0164}", "\u{0165}", "C");//C
testCaseEquivalence("\u{0166}", "\u{0167}", "C");//C
testCaseEquivalence("\u{0168}", "\u{0169}", "C");//C
testCaseEquivalence("\u{016A}", "\u{016B}", "C");//C
testCaseEquivalence("\u{016C}", "\u{016D}", "C");//C
testCaseEquivalence("\u{016E}", "\u{016F}", "C");//C
testCaseEquivalence("\u{0170}", "\u{0171}", "C");//C
testCaseEquivalence("\u{0172}", "\u{0173}", "C");//C
testCaseEquivalence("\u{0174}", "\u{0175}", "C");//C
testCaseEquivalence("\u{0176}", "\u{0177}", "C");//C
testCaseEquivalence("\u{0178}", "\u{00FF}", "C");//C
testCaseEquivalence("\u{0179}", "\u{017A}", "C");//C
testCaseEquivalence("\u{017B}", "\u{017C}", "C");//C
testCaseEquivalence("\u{017D}", "\u{017E}", "C");//C
testCaseEquivalence("\u{017F}", "\u{0073}", "C");//C
testCaseEquivalence("\u{0181}", "\u{0253}", "C");//C
testCaseEquivalence("\u{0182}", "\u{0183}", "C");//C
testCaseEquivalence("\u{0184}", "\u{0185}", "C");//C
testCaseEquivalence("\u{0186}", "\u{0254}", "C");//C
testCaseEquivalence("\u{0187}", "\u{0188}", "C");//C
testCaseEquivalence("\u{0189}", "\u{0256}", "C");//C
testCaseEquivalence("\u{018A}", "\u{0257}", "C");//C
testCaseEquivalence("\u{018B}", "\u{018C}", "C");//C
testCaseEquivalence("\u{018E}", "\u{01DD}", "C");//C
testCaseEquivalence("\u{018F}", "\u{0259}", "C");//C
testCaseEquivalence("\u{0190}", "\u{025B}", "C");//C
testCaseEquivalence("\u{0191}", "\u{0192}", "C");//C
testCaseEquivalence("\u{0193}", "\u{0260}", "C");//C
testCaseEquivalence("\u{0194}", "\u{0263}", "C");//C
testCaseEquivalence("\u{0196}", "\u{0269}", "C");//C
testCaseEquivalence("\u{0197}", "\u{0268}", "C");//C
testCaseEquivalence("\u{0198}", "\u{0199}", "C");//C
testCaseEquivalence("\u{019C}", "\u{026F}", "C");//C
testCaseEquivalence("\u{019D}", "\u{0272}", "C");//C
testCaseEquivalence("\u{019F}", "\u{0275}", "C");//C
testCaseEquivalence("\u{01A0}", "\u{01A1}", "C");//C
testCaseEquivalence("\u{01A2}", "\u{01A3}", "C");//C
testCaseEquivalence("\u{01A4}", "\u{01A5}", "C");//C
testCaseEquivalence("\u{01A6}", "\u{0280}", "C");//C
testCaseEquivalence("\u{01A7}", "\u{01A8}", "C");//C
testCaseEquivalence("\u{01A9}", "\u{0283}", "C");//C
testCaseEquivalence("\u{01AC}", "\u{01AD}", "C");//C
testCaseEquivalence("\u{01AE}", "\u{0288}", "C");//C
testCaseEquivalence("\u{01AF}", "\u{01B0}", "C");//C
testCaseEquivalence("\u{01B1}", "\u{028A}", "C");//C
testCaseEquivalence("\u{01B2}", "\u{028B}", "C");//C
testCaseEquivalence("\u{01B3}", "\u{01B4}", "C");//C
testCaseEquivalence("\u{01B5}", "\u{01B6}", "C");//C
testCaseEquivalence("\u{01B7}", "\u{0292}", "C");//C
testCaseEquivalence("\u{01B8}", "\u{01B9}", "C");//C
testCaseEquivalence("\u{01BC}", "\u{01BD}", "C");//C
testCaseEquivalence("\u{01C4}", "\u{01C6}", "C");//C
testCaseEquivalence("\u{01C5}", "\u{01C6}", "C");//C
testCaseEquivalence("\u{01C7}", "\u{01C9}", "C");//C
testCaseEquivalence("\u{01C8}", "\u{01C9}", "C");//C
testCaseEquivalence("\u{01CA}", "\u{01CC}", "C");//C
testCaseEquivalence("\u{01CB}", "\u{01CC}", "C");//C
testCaseEquivalence("\u{01CD}", "\u{01CE}", "C");//C
testCaseEquivalence("\u{01CF}", "\u{01D0}", "C");//C
testCaseEquivalence("\u{01D1}", "\u{01D2}", "C");//C
testCaseEquivalence("\u{01D3}", "\u{01D4}", "C");//C
testCaseEquivalence("\u{01D5}", "\u{01D6}", "C");//C
testCaseEquivalence("\u{01D7}", "\u{01D8}", "C");//C
testCaseEquivalence("\u{01D9}", "\u{01DA}", "C");//C
testCaseEquivalence("\u{01DB}", "\u{01DC}", "C");//C
testCaseEquivalence("\u{01DE}", "\u{01DF}", "C");//C
testCaseEquivalence("\u{01E0}", "\u{01E1}", "C");//C
testCaseEquivalence("\u{01E2}", "\u{01E3}", "C");//C
testCaseEquivalence("\u{01E4}", "\u{01E5}", "C");//C
testCaseEquivalence("\u{01E6}", "\u{01E7}", "C");//C
testCaseEquivalence("\u{01E8}", "\u{01E9}", "C");//C
testCaseEquivalence("\u{01EA}", "\u{01EB}", "C");//C
testCaseEquivalence("\u{01EC}", "\u{01ED}", "C");//C
testCaseEquivalence("\u{01EE}", "\u{01EF}", "C");//C
testCaseEquivalence("\u{01F1}", "\u{01F3}", "C");//C
testCaseEquivalence("\u{01F2}", "\u{01F3}", "C");//C
testCaseEquivalence("\u{01F4}", "\u{01F5}", "C");//C
testCaseEquivalence("\u{01F6}", "\u{0195}", "C");//C
testCaseEquivalence("\u{01F7}", "\u{01BF}", "C");//C
testCaseEquivalence("\u{01F8}", "\u{01F9}", "C");//C
testCaseEquivalence("\u{01FA}", "\u{01FB}", "C");//C
testCaseEquivalence("\u{01FC}", "\u{01FD}", "C");//C
testCaseEquivalence("\u{01FE}", "\u{01FF}", "C");//C
testCaseEquivalence("\u{0200}", "\u{0201}", "C");//C
testCaseEquivalence("\u{0202}", "\u{0203}", "C");//C
testCaseEquivalence("\u{0204}", "\u{0205}", "C");//C
testCaseEquivalence("\u{0206}", "\u{0207}", "C");//C
testCaseEquivalence("\u{0208}", "\u{0209}", "C");//C
testCaseEquivalence("\u{020A}", "\u{020B}", "C");//C
testCaseEquivalence("\u{020C}", "\u{020D}", "C");//C
testCaseEquivalence("\u{020E}", "\u{020F}", "C");//C
testCaseEquivalence("\u{0210}", "\u{0211}", "C");//C
testCaseEquivalence("\u{0212}", "\u{0213}", "C");//C
testCaseEquivalence("\u{0214}", "\u{0215}", "C");//C
testCaseEquivalence("\u{0216}", "\u{0217}", "C");//C
testCaseEquivalence("\u{0218}", "\u{0219}", "C");//C
testCaseEquivalence("\u{021A}", "\u{021B}", "C");//C
testCaseEquivalence("\u{021C}", "\u{021D}", "C");//C
testCaseEquivalence("\u{021E}", "\u{021F}", "C");//C
testCaseEquivalence("\u{0220}", "\u{019E}", "C");//C
testCaseEquivalence("\u{0222}", "\u{0223}", "C");//C
testCaseEquivalence("\u{0224}", "\u{0225}", "C");//C
testCaseEquivalence("\u{0226}", "\u{0227}", "C");//C
testCaseEquivalence("\u{0228}", "\u{0229}", "C");//C
testCaseEquivalence("\u{022A}", "\u{022B}", "C");//C
testCaseEquivalence("\u{022C}", "\u{022D}", "C");//C
testCaseEquivalence("\u{022E}", "\u{022F}", "C");//C
testCaseEquivalence("\u{0230}", "\u{0231}", "C");//C
testCaseEquivalence("\u{0232}", "\u{0233}", "C");//C
testCaseEquivalence("\u{023A}", "\u{2C65}", "C");//C
testCaseEquivalence("\u{023B}", "\u{023C}", "C");//C
testCaseEquivalence("\u{023D}", "\u{019A}", "C");//C
testCaseEquivalence("\u{023E}", "\u{2C66}", "C");//C
testCaseEquivalence("\u{0241}", "\u{0242}", "C");//C
testCaseEquivalence("\u{0243}", "\u{0180}", "C");//C
testCaseEquivalence("\u{0244}", "\u{0289}", "C");//C
testCaseEquivalence("\u{0245}", "\u{028C}", "C");//C
testCaseEquivalence("\u{0246}", "\u{0247}", "C");//C
testCaseEquivalence("\u{0248}", "\u{0249}", "C");//C
testCaseEquivalence("\u{024A}", "\u{024B}", "C");//C
testCaseEquivalence("\u{024C}", "\u{024D}", "C");//C
testCaseEquivalence("\u{024E}", "\u{024F}", "C");//C
testCaseEquivalence("\u{0345}", "\u{03B9}", "C");//C
testCaseEquivalence("\u{0370}", "\u{0371}", "C");//C
testCaseEquivalence("\u{0372}", "\u{0373}", "C");//C
testCaseEquivalence("\u{0376}", "\u{0377}", "C");//C
testCaseEquivalence("\u{0386}", "\u{03AC}", "C");//C
testCaseEquivalence("\u{0388}", "\u{03AD}", "C");//C
testCaseEquivalence("\u{0389}", "\u{03AE}", "C");//C
testCaseEquivalence("\u{038A}", "\u{03AF}", "C");//C
testCaseEquivalence("\u{038C}", "\u{03CC}", "C");//C
testCaseEquivalence("\u{038E}", "\u{03CD}", "C");//C
testCaseEquivalence("\u{038F}", "\u{03CE}", "C");//C
testCaseEquivalence("\u{0391}", "\u{03B1}", "C");//C
testCaseEquivalence("\u{0392}", "\u{03B2}", "C");//C
testCaseEquivalence("\u{0393}", "\u{03B3}", "C");//C
testCaseEquivalence("\u{0394}", "\u{03B4}", "C");//C
testCaseEquivalence("\u{0395}", "\u{03B5}", "C");//C
testCaseEquivalence("\u{0396}", "\u{03B6}", "C");//C
testCaseEquivalence("\u{0397}", "\u{03B7}", "C");//C
testCaseEquivalence("\u{0398}", "\u{03B8}", "C");//C
testCaseEquivalence("\u{0399}", "\u{03B9}", "C");//C
testCaseEquivalence("\u{039A}", "\u{03BA}", "C");//C
testCaseEquivalence("\u{039B}", "\u{03BB}", "C");//C
testCaseEquivalence("\u{039C}", "\u{03BC}", "C");//C
testCaseEquivalence("\u{039D}", "\u{03BD}", "C");//C
testCaseEquivalence("\u{039E}", "\u{03BE}", "C");//C
testCaseEquivalence("\u{039F}", "\u{03BF}", "C");//C
testCaseEquivalence("\u{03A0}", "\u{03C0}", "C");//C
testCaseEquivalence("\u{03A1}", "\u{03C1}", "C");//C
testCaseEquivalence("\u{03A3}", "\u{03C3}", "C");//C
testCaseEquivalence("\u{03A4}", "\u{03C4}", "C");//C
testCaseEquivalence("\u{03A5}", "\u{03C5}", "C");//C
testCaseEquivalence("\u{03A6}", "\u{03C6}", "C");//C
testCaseEquivalence("\u{03A7}", "\u{03C7}", "C");//C
testCaseEquivalence("\u{03A8}", "\u{03C8}", "C");//C
testCaseEquivalence("\u{03A9}", "\u{03C9}", "C");//C
testCaseEquivalence("\u{03AA}", "\u{03CA}", "C");//C
testCaseEquivalence("\u{03AB}", "\u{03CB}", "C");//C
testCaseEquivalence("\u{03C2}", "\u{03C3}", "C");//C
testCaseEquivalence("\u{03CF}", "\u{03D7}", "C");//C
testCaseEquivalence("\u{03D0}", "\u{03B2}", "C");//C
testCaseEquivalence("\u{03D1}", "\u{03B8}", "C");//C
testCaseEquivalence("\u{03D5}", "\u{03C6}", "C");//C
testCaseEquivalence("\u{03D6}", "\u{03C0}", "C");//C
testCaseEquivalence("\u{03D8}", "\u{03D9}", "C");//C
testCaseEquivalence("\u{03DA}", "\u{03DB}", "C");//C
testCaseEquivalence("\u{03DC}", "\u{03DD}", "C");//C
testCaseEquivalence("\u{03DE}", "\u{03DF}", "C");//C
testCaseEquivalence("\u{03E0}", "\u{03E1}", "C");//C
testCaseEquivalence("\u{03E2}", "\u{03E3}", "C");//C
testCaseEquivalence("\u{03E4}", "\u{03E5}", "C");//C
testCaseEquivalence("\u{03E6}", "\u{03E7}", "C");//C
testCaseEquivalence("\u{03E8}", "\u{03E9}", "C");//C
testCaseEquivalence("\u{03EA}", "\u{03EB}", "C");//C
testCaseEquivalence("\u{03EC}", "\u{03ED}", "C");//C
testCaseEquivalence("\u{03EE}", "\u{03EF}", "C");//C
testCaseEquivalence("\u{03F0}", "\u{03BA}", "C");//C
testCaseEquivalence("\u{03F1}", "\u{03C1}", "C");//C
testCaseEquivalence("\u{03F4}", "\u{03B8}", "C");//C
testCaseEquivalence("\u{03F5}", "\u{03B5}", "C");//C
testCaseEquivalence("\u{03F7}", "\u{03F8}", "C");//C
testCaseEquivalence("\u{03F9}", "\u{03F2}", "C");//C
testCaseEquivalence("\u{03FA}", "\u{03FB}", "C");//C
testCaseEquivalence("\u{03FD}", "\u{037B}", "C");//C
testCaseEquivalence("\u{03FE}", "\u{037C}", "C");//C
testCaseEquivalence("\u{03FF}", "\u{037D}", "C");//C
testCaseEquivalence("\u{0400}", "\u{0450}", "C");//C
testCaseEquivalence("\u{0401}", "\u{0451}", "C");//C
testCaseEquivalence("\u{0402}", "\u{0452}", "C");//C
testCaseEquivalence("\u{0403}", "\u{0453}", "C");//C
testCaseEquivalence("\u{0404}", "\u{0454}", "C");//C
testCaseEquivalence("\u{0405}", "\u{0455}", "C");//C
testCaseEquivalence("\u{0406}", "\u{0456}", "C");//C
testCaseEquivalence("\u{0407}", "\u{0457}", "C");//C
testCaseEquivalence("\u{0408}", "\u{0458}", "C");//C
testCaseEquivalence("\u{0409}", "\u{0459}", "C");//C
testCaseEquivalence("\u{040A}", "\u{045A}", "C");//C
testCaseEquivalence("\u{040B}", "\u{045B}", "C");//C
testCaseEquivalence("\u{040C}", "\u{045C}", "C");//C
testCaseEquivalence("\u{040D}", "\u{045D}", "C");//C
testCaseEquivalence("\u{040E}", "\u{045E}", "C");//C
testCaseEquivalence("\u{040F}", "\u{045F}", "C");//C
testCaseEquivalence("\u{0410}", "\u{0430}", "C");//C
testCaseEquivalence("\u{0411}", "\u{0431}", "C");//C
testCaseEquivalence("\u{0412}", "\u{0432}", "C");//C
testCaseEquivalence("\u{0413}", "\u{0433}", "C");//C
testCaseEquivalence("\u{0414}", "\u{0434}", "C");//C
testCaseEquivalence("\u{0415}", "\u{0435}", "C");//C
testCaseEquivalence("\u{0416}", "\u{0436}", "C");//C
testCaseEquivalence("\u{0417}", "\u{0437}", "C");//C
testCaseEquivalence("\u{0418}", "\u{0438}", "C");//C
testCaseEquivalence("\u{0419}", "\u{0439}", "C");//C
testCaseEquivalence("\u{041A}", "\u{043A}", "C");//C
testCaseEquivalence("\u{041B}", "\u{043B}", "C");//C
testCaseEquivalence("\u{041C}", "\u{043C}", "C");//C
testCaseEquivalence("\u{041D}", "\u{043D}", "C");//C
testCaseEquivalence("\u{041E}", "\u{043E}", "C");//C
testCaseEquivalence("\u{041F}", "\u{043F}", "C");//C
testCaseEquivalence("\u{0420}", "\u{0440}", "C");//C
testCaseEquivalence("\u{0421}", "\u{0441}", "C");//C
testCaseEquivalence("\u{0422}", "\u{0442}", "C");//C
testCaseEquivalence("\u{0423}", "\u{0443}", "C");//C
testCaseEquivalence("\u{0424}", "\u{0444}", "C");//C
testCaseEquivalence("\u{0425}", "\u{0445}", "C");//C
testCaseEquivalence("\u{0426}", "\u{0446}", "C");//C
testCaseEquivalence("\u{0427}", "\u{0447}", "C");//C
testCaseEquivalence("\u{0428}", "\u{0448}", "C");//C
testCaseEquivalence("\u{0429}", "\u{0449}", "C");//C
testCaseEquivalence("\u{042A}", "\u{044A}", "C");//C
testCaseEquivalence("\u{042B}", "\u{044B}", "C");//C
testCaseEquivalence("\u{042C}", "\u{044C}", "C");//C
testCaseEquivalence("\u{042D}", "\u{044D}", "C");//C
testCaseEquivalence("\u{042E}", "\u{044E}", "C");//C
testCaseEquivalence("\u{042F}", "\u{044F}", "C");//C
testCaseEquivalence("\u{0460}", "\u{0461}", "C");//C
testCaseEquivalence("\u{0462}", "\u{0463}", "C");//C
testCaseEquivalence("\u{0464}", "\u{0465}", "C");//C
testCaseEquivalence("\u{0466}", "\u{0467}", "C");//C
testCaseEquivalence("\u{0468}", "\u{0469}", "C");//C
testCaseEquivalence("\u{046A}", "\u{046B}", "C");//C
testCaseEquivalence("\u{046C}", "\u{046D}", "C");//C
testCaseEquivalence("\u{046E}", "\u{046F}", "C");//C
testCaseEquivalence("\u{0470}", "\u{0471}", "C");//C
testCaseEquivalence("\u{0472}", "\u{0473}", "C");//C
testCaseEquivalence("\u{0474}", "\u{0475}", "C");//C
testCaseEquivalence("\u{0476}", "\u{0477}", "C");//C
testCaseEquivalence("\u{0478}", "\u{0479}", "C");//C
testCaseEquivalence("\u{047A}", "\u{047B}", "C");//C
testCaseEquivalence("\u{047C}", "\u{047D}", "C");//C
testCaseEquivalence("\u{047E}", "\u{047F}", "C");//C
testCaseEquivalence("\u{0480}", "\u{0481}", "C");//C
testCaseEquivalence("\u{048A}", "\u{048B}", "C");//C
testCaseEquivalence("\u{048C}", "\u{048D}", "C");//C
testCaseEquivalence("\u{048E}", "\u{048F}", "C");//C
testCaseEquivalence("\u{0490}", "\u{0491}", "C");//C
testCaseEquivalence("\u{0492}", "\u{0493}", "C");//C
testCaseEquivalence("\u{0494}", "\u{0495}", "C");//C
testCaseEquivalence("\u{0496}", "\u{0497}", "C");//C
testCaseEquivalence("\u{0498}", "\u{0499}", "C");//C
testCaseEquivalence("\u{049A}", "\u{049B}", "C");//C
testCaseEquivalence("\u{049C}", "\u{049D}", "C");//C
testCaseEquivalence("\u{049E}", "\u{049F}", "C");//C
testCaseEquivalence("\u{04A0}", "\u{04A1}", "C");//C
testCaseEquivalence("\u{04A2}", "\u{04A3}", "C");//C
testCaseEquivalence("\u{04A4}", "\u{04A5}", "C");//C
testCaseEquivalence("\u{04A6}", "\u{04A7}", "C");//C
testCaseEquivalence("\u{04A8}", "\u{04A9}", "C");//C
testCaseEquivalence("\u{04AA}", "\u{04AB}", "C");//C
testCaseEquivalence("\u{04AC}", "\u{04AD}", "C");//C
testCaseEquivalence("\u{04AE}", "\u{04AF}", "C");//C
testCaseEquivalence("\u{04B0}", "\u{04B1}", "C");//C
testCaseEquivalence("\u{04B2}", "\u{04B3}", "C");//C
testCaseEquivalence("\u{04B4}", "\u{04B5}", "C");//C
testCaseEquivalence("\u{04B6}", "\u{04B7}", "C");//C
testCaseEquivalence("\u{04B8}", "\u{04B9}", "C");//C
testCaseEquivalence("\u{04BA}", "\u{04BB}", "C");//C
testCaseEquivalence("\u{04BC}", "\u{04BD}", "C");//C
testCaseEquivalence("\u{04BE}", "\u{04BF}", "C");//C
testCaseEquivalence("\u{04C0}", "\u{04CF}", "C");//C
testCaseEquivalence("\u{04C1}", "\u{04C2}", "C");//C
testCaseEquivalence("\u{04C3}", "\u{04C4}", "C");//C
testCaseEquivalence("\u{04C5}", "\u{04C6}", "C");//C
testCaseEquivalence("\u{04C7}", "\u{04C8}", "C");//C
testCaseEquivalence("\u{04C9}", "\u{04CA}", "C");//C
testCaseEquivalence("\u{04CB}", "\u{04CC}", "C");//C
testCaseEquivalence("\u{04CD}", "\u{04CE}", "C");//C
testCaseEquivalence("\u{04D0}", "\u{04D1}", "C");//C
testCaseEquivalence("\u{04D2}", "\u{04D3}", "C");//C
testCaseEquivalence("\u{04D4}", "\u{04D5}", "C");//C
testCaseEquivalence("\u{04D6}", "\u{04D7}", "C");//C
testCaseEquivalence("\u{04D8}", "\u{04D9}", "C");//C
testCaseEquivalence("\u{04DA}", "\u{04DB}", "C");//C
testCaseEquivalence("\u{04DC}", "\u{04DD}", "C");//C
testCaseEquivalence("\u{04DE}", "\u{04DF}", "C");//C
testCaseEquivalence("\u{04E0}", "\u{04E1}", "C");//C
testCaseEquivalence("\u{04E2}", "\u{04E3}", "C");//C
testCaseEquivalence("\u{04E4}", "\u{04E5}", "C");//C
testCaseEquivalence("\u{04E6}", "\u{04E7}", "C");//C
testCaseEquivalence("\u{04E8}", "\u{04E9}", "C");//C
testCaseEquivalence("\u{04EA}", "\u{04EB}", "C");//C
testCaseEquivalence("\u{04EC}", "\u{04ED}", "C");//C
testCaseEquivalence("\u{04EE}", "\u{04EF}", "C");//C
testCaseEquivalence("\u{04F0}", "\u{04F1}", "C");//C
testCaseEquivalence("\u{04F2}", "\u{04F3}", "C");//C
testCaseEquivalence("\u{04F4}", "\u{04F5}", "C");//C
testCaseEquivalence("\u{04F6}", "\u{04F7}", "C");//C
testCaseEquivalence("\u{04F8}", "\u{04F9}", "C");//C
testCaseEquivalence("\u{04FA}", "\u{04FB}", "C");//C
testCaseEquivalence("\u{04FC}", "\u{04FD}", "C");//C
testCaseEquivalence("\u{04FE}", "\u{04FF}", "C");//C
testCaseEquivalence("\u{0500}", "\u{0501}", "C");//C
testCaseEquivalence("\u{0502}", "\u{0503}", "C");//C
testCaseEquivalence("\u{0504}", "\u{0505}", "C");//C
testCaseEquivalence("\u{0506}", "\u{0507}", "C");//C
testCaseEquivalence("\u{0508}", "\u{0509}", "C");//C
testCaseEquivalence("\u{050A}", "\u{050B}", "C");//C
testCaseEquivalence("\u{050C}", "\u{050D}", "C");//C
testCaseEquivalence("\u{050E}", "\u{050F}", "C");//C
testCaseEquivalence("\u{0510}", "\u{0511}", "C");//C
testCaseEquivalence("\u{0512}", "\u{0513}", "C");//C
testCaseEquivalence("\u{0514}", "\u{0515}", "C");//C
testCaseEquivalence("\u{0516}", "\u{0517}", "C");//C
testCaseEquivalence("\u{0518}", "\u{0519}", "C");//C
testCaseEquivalence("\u{051A}", "\u{051B}", "C");//C
testCaseEquivalence("\u{051C}", "\u{051D}", "C");//C
testCaseEquivalence("\u{051E}", "\u{051F}", "C");//C
testCaseEquivalence("\u{0520}", "\u{0521}", "C");//C
testCaseEquivalence("\u{0522}", "\u{0523}", "C");//C
testCaseEquivalence("\u{0524}", "\u{0525}", "C");//C
testCaseEquivalence("\u{0526}", "\u{0527}", "C");//C
testCaseEquivalence("\u{0531}", "\u{0561}", "C");//C
testCaseEquivalence("\u{0532}", "\u{0562}", "C");//C
testCaseEquivalence("\u{0533}", "\u{0563}", "C");//C
testCaseEquivalence("\u{0534}", "\u{0564}", "C");//C
testCaseEquivalence("\u{0535}", "\u{0565}", "C");//C
testCaseEquivalence("\u{0536}", "\u{0566}", "C");//C
testCaseEquivalence("\u{0537}", "\u{0567}", "C");//C
testCaseEquivalence("\u{0538}", "\u{0568}", "C");//C
testCaseEquivalence("\u{0539}", "\u{0569}", "C");//C
testCaseEquivalence("\u{053A}", "\u{056A}", "C");//C
testCaseEquivalence("\u{053B}", "\u{056B}", "C");//C
testCaseEquivalence("\u{053C}", "\u{056C}", "C");//C
testCaseEquivalence("\u{053D}", "\u{056D}", "C");//C
testCaseEquivalence("\u{053E}", "\u{056E}", "C");//C
testCaseEquivalence("\u{053F}", "\u{056F}", "C");//C
testCaseEquivalence("\u{0540}", "\u{0570}", "C");//C
testCaseEquivalence("\u{0541}", "\u{0571}", "C");//C
testCaseEquivalence("\u{0542}", "\u{0572}", "C");//C
testCaseEquivalence("\u{0543}", "\u{0573}", "C");//C
testCaseEquivalence("\u{0544}", "\u{0574}", "C");//C
testCaseEquivalence("\u{0545}", "\u{0575}", "C");//C
testCaseEquivalence("\u{0546}", "\u{0576}", "C");//C
testCaseEquivalence("\u{0547}", "\u{0577}", "C");//C
testCaseEquivalence("\u{0548}", "\u{0578}", "C");//C
testCaseEquivalence("\u{0549}", "\u{0579}", "C");//C
testCaseEquivalence("\u{054A}", "\u{057A}", "C");//C
testCaseEquivalence("\u{054B}", "\u{057B}", "C");//C
testCaseEquivalence("\u{054C}", "\u{057C}", "C");//C
testCaseEquivalence("\u{054D}", "\u{057D}", "C");//C
testCaseEquivalence("\u{054E}", "\u{057E}", "C");//C
testCaseEquivalence("\u{054F}", "\u{057F}", "C");//C
testCaseEquivalence("\u{0550}", "\u{0580}", "C");//C
testCaseEquivalence("\u{0551}", "\u{0581}", "C");//C
testCaseEquivalence("\u{0552}", "\u{0582}", "C");//C
testCaseEquivalence("\u{0553}", "\u{0583}", "C");//C
testCaseEquivalence("\u{0554}", "\u{0584}", "C");//C
testCaseEquivalence("\u{0555}", "\u{0585}", "C");//C
testCaseEquivalence("\u{0556}", "\u{0586}", "C");//C
testCaseEquivalence("\u{10A0}", "\u{2D00}", "C");//C
testCaseEquivalence("\u{10A1}", "\u{2D01}", "C");//C
testCaseEquivalence("\u{10A2}", "\u{2D02}", "C");//C
testCaseEquivalence("\u{10A3}", "\u{2D03}", "C");//C
testCaseEquivalence("\u{10A4}", "\u{2D04}", "C");//C
testCaseEquivalence("\u{10A5}", "\u{2D05}", "C");//C
testCaseEquivalence("\u{10A6}", "\u{2D06}", "C");//C
testCaseEquivalence("\u{10A7}", "\u{2D07}", "C");//C
testCaseEquivalence("\u{10A8}", "\u{2D08}", "C");//C
testCaseEquivalence("\u{10A9}", "\u{2D09}", "C");//C
testCaseEquivalence("\u{10AA}", "\u{2D0A}", "C");//C
testCaseEquivalence("\u{10AB}", "\u{2D0B}", "C");//C
testCaseEquivalence("\u{10AC}", "\u{2D0C}", "C");//C
testCaseEquivalence("\u{10AD}", "\u{2D0D}", "C");//C
testCaseEquivalence("\u{10AE}", "\u{2D0E}", "C");//C
testCaseEquivalence("\u{10AF}", "\u{2D0F}", "C");//C
testCaseEquivalence("\u{10B0}", "\u{2D10}", "C");//C
testCaseEquivalence("\u{10B1}", "\u{2D11}", "C");//C
testCaseEquivalence("\u{10B2}", "\u{2D12}", "C");//C
testCaseEquivalence("\u{10B3}", "\u{2D13}", "C");//C
testCaseEquivalence("\u{10B4}", "\u{2D14}", "C");//C
testCaseEquivalence("\u{10B5}", "\u{2D15}", "C");//C
testCaseEquivalence("\u{10B6}", "\u{2D16}", "C");//C
testCaseEquivalence("\u{10B7}", "\u{2D17}", "C");//C
testCaseEquivalence("\u{10B8}", "\u{2D18}", "C");//C
testCaseEquivalence("\u{10B9}", "\u{2D19}", "C");//C
testCaseEquivalence("\u{10BA}", "\u{2D1A}", "C");//C
testCaseEquivalence("\u{10BB}", "\u{2D1B}", "C");//C
testCaseEquivalence("\u{10BC}", "\u{2D1C}", "C");//C
testCaseEquivalence("\u{10BD}", "\u{2D1D}", "C");//C
testCaseEquivalence("\u{10BE}", "\u{2D1E}", "C");//C
testCaseEquivalence("\u{10BF}", "\u{2D1F}", "C");//C
testCaseEquivalence("\u{10C0}", "\u{2D20}", "C");//C
testCaseEquivalence("\u{10C1}", "\u{2D21}", "C");//C
testCaseEquivalence("\u{10C2}", "\u{2D22}", "C");//C
testCaseEquivalence("\u{10C3}", "\u{2D23}", "C");//C
testCaseEquivalence("\u{10C4}", "\u{2D24}", "C");//C
testCaseEquivalence("\u{10C5}", "\u{2D25}", "C");//C
testCaseEquivalence("\u{10C7}", "\u{2D27}", "C");//C
testCaseEquivalence("\u{10CD}", "\u{2D2D}", "C");//C
testCaseEquivalence("\u{1E00}", "\u{1E01}", "C");//C
testCaseEquivalence("\u{1E02}", "\u{1E03}", "C");//C
testCaseEquivalence("\u{1E04}", "\u{1E05}", "C");//C
testCaseEquivalence("\u{1E06}", "\u{1E07}", "C");//C
testCaseEquivalence("\u{1E08}", "\u{1E09}", "C");//C
testCaseEquivalence("\u{1E0A}", "\u{1E0B}", "C");//C
testCaseEquivalence("\u{1E0C}", "\u{1E0D}", "C");//C
testCaseEquivalence("\u{1E0E}", "\u{1E0F}", "C");//C
testCaseEquivalence("\u{1E10}", "\u{1E11}", "C");//C
testCaseEquivalence("\u{1E12}", "\u{1E13}", "C");//C
testCaseEquivalence("\u{1E14}", "\u{1E15}", "C");//C
testCaseEquivalence("\u{1E16}", "\u{1E17}", "C");//C
testCaseEquivalence("\u{1E18}", "\u{1E19}", "C");//C
testCaseEquivalence("\u{1E1A}", "\u{1E1B}", "C");//C
testCaseEquivalence("\u{1E1C}", "\u{1E1D}", "C");//C
testCaseEquivalence("\u{1E1E}", "\u{1E1F}", "C");//C
testCaseEquivalence("\u{1E20}", "\u{1E21}", "C");//C
testCaseEquivalence("\u{1E22}", "\u{1E23}", "C");//C
testCaseEquivalence("\u{1E24}", "\u{1E25}", "C");//C
testCaseEquivalence("\u{1E26}", "\u{1E27}", "C");//C
testCaseEquivalence("\u{1E28}", "\u{1E29}", "C");//C
testCaseEquivalence("\u{1E2A}", "\u{1E2B}", "C");//C
testCaseEquivalence("\u{1E2C}", "\u{1E2D}", "C");//C
testCaseEquivalence("\u{1E2E}", "\u{1E2F}", "C");//C
testCaseEquivalence("\u{1E30}", "\u{1E31}", "C");//C
testCaseEquivalence("\u{1E32}", "\u{1E33}", "C");//C
testCaseEquivalence("\u{1E34}", "\u{1E35}", "C");//C
testCaseEquivalence("\u{1E36}", "\u{1E37}", "C");//C
testCaseEquivalence("\u{1E38}", "\u{1E39}", "C");//C
testCaseEquivalence("\u{1E3A}", "\u{1E3B}", "C");//C
testCaseEquivalence("\u{1E3C}", "\u{1E3D}", "C");//C
testCaseEquivalence("\u{1E3E}", "\u{1E3F}", "C");//C
testCaseEquivalence("\u{1E40}", "\u{1E41}", "C");//C
testCaseEquivalence("\u{1E42}", "\u{1E43}", "C");//C
testCaseEquivalence("\u{1E44}", "\u{1E45}", "C");//C
testCaseEquivalence("\u{1E46}", "\u{1E47}", "C");//C
testCaseEquivalence("\u{1E48}", "\u{1E49}", "C");//C
testCaseEquivalence("\u{1E4A}", "\u{1E4B}", "C");//C
testCaseEquivalence("\u{1E4C}", "\u{1E4D}", "C");//C
testCaseEquivalence("\u{1E4E}", "\u{1E4F}", "C");//C
testCaseEquivalence("\u{1E50}", "\u{1E51}", "C");//C
testCaseEquivalence("\u{1E52}", "\u{1E53}", "C");//C
testCaseEquivalence("\u{1E54}", "\u{1E55}", "C");//C
testCaseEquivalence("\u{1E56}", "\u{1E57}", "C");//C
testCaseEquivalence("\u{1E58}", "\u{1E59}", "C");//C
testCaseEquivalence("\u{1E5A}", "\u{1E5B}", "C");//C
testCaseEquivalence("\u{1E5C}", "\u{1E5D}", "C");//C
testCaseEquivalence("\u{1E5E}", "\u{1E5F}", "C");//C
testCaseEquivalence("\u{1E60}", "\u{1E61}", "C");//C
testCaseEquivalence("\u{1E62}", "\u{1E63}", "C");//C
testCaseEquivalence("\u{1E64}", "\u{1E65}", "C");//C
testCaseEquivalence("\u{1E66}", "\u{1E67}", "C");//C
testCaseEquivalence("\u{1E68}", "\u{1E69}", "C");//C
testCaseEquivalence("\u{1E6A}", "\u{1E6B}", "C");//C
testCaseEquivalence("\u{1E6C}", "\u{1E6D}", "C");//C
testCaseEquivalence("\u{1E6E}", "\u{1E6F}", "C");//C
testCaseEquivalence("\u{1E70}", "\u{1E71}", "C");//C
testCaseEquivalence("\u{1E72}", "\u{1E73}", "C");//C
testCaseEquivalence("\u{1E74}", "\u{1E75}", "C");//C
testCaseEquivalence("\u{1E76}", "\u{1E77}", "C");//C
testCaseEquivalence("\u{1E78}", "\u{1E79}", "C");//C
testCaseEquivalence("\u{1E7A}", "\u{1E7B}", "C");//C
testCaseEquivalence("\u{1E7C}", "\u{1E7D}", "C");//C
testCaseEquivalence("\u{1E7E}", "\u{1E7F}", "C");//C
testCaseEquivalence("\u{1E80}", "\u{1E81}", "C");//C
testCaseEquivalence("\u{1E82}", "\u{1E83}", "C");//C
testCaseEquivalence("\u{1E84}", "\u{1E85}", "C");//C
testCaseEquivalence("\u{1E86}", "\u{1E87}", "C");//C
testCaseEquivalence("\u{1E88}", "\u{1E89}", "C");//C
testCaseEquivalence("\u{1E8A}", "\u{1E8B}", "C");//C
testCaseEquivalence("\u{1E8C}", "\u{1E8D}", "C");//C
testCaseEquivalence("\u{1E8E}", "\u{1E8F}", "C");//C
testCaseEquivalence("\u{1E90}", "\u{1E91}", "C");//C
testCaseEquivalence("\u{1E92}", "\u{1E93}", "C");//C
testCaseEquivalence("\u{1E94}", "\u{1E95}", "C");//C
testCaseEquivalence("\u{1E9B}", "\u{1E61}", "C");//C
testCaseEquivalence("\u{1E9E}", "\u{00DF}", "S");//S
testCaseEquivalence("\u{1EA0}", "\u{1EA1}", "C");//C
testCaseEquivalence("\u{1EA2}", "\u{1EA3}", "C");//C
testCaseEquivalence("\u{1EA4}", "\u{1EA5}", "C");//C
testCaseEquivalence("\u{1EA6}", "\u{1EA7}", "C");//C
testCaseEquivalence("\u{1EA8}", "\u{1EA9}", "C");//C
testCaseEquivalence("\u{1EAA}", "\u{1EAB}", "C");//C
testCaseEquivalence("\u{1EAC}", "\u{1EAD}", "C");//C
testCaseEquivalence("\u{1EAE}", "\u{1EAF}", "C");//C
testCaseEquivalence("\u{1EB0}", "\u{1EB1}", "C");//C
testCaseEquivalence("\u{1EB2}", "\u{1EB3}", "C");//C
testCaseEquivalence("\u{1EB4}", "\u{1EB5}", "C");//C
testCaseEquivalence("\u{1EB6}", "\u{1EB7}", "C");//C
testCaseEquivalence("\u{1EB8}", "\u{1EB9}", "C");//C
testCaseEquivalence("\u{1EBA}", "\u{1EBB}", "C");//C
testCaseEquivalence("\u{1EBC}", "\u{1EBD}", "C");//C
testCaseEquivalence("\u{1EBE}", "\u{1EBF}", "C");//C
testCaseEquivalence("\u{1EC0}", "\u{1EC1}", "C");//C
testCaseEquivalence("\u{1EC2}", "\u{1EC3}", "C");//C
testCaseEquivalence("\u{1EC4}", "\u{1EC5}", "C");//C
testCaseEquivalence("\u{1EC6}", "\u{1EC7}", "C");//C
testCaseEquivalence("\u{1EC8}", "\u{1EC9}", "C");//C
testCaseEquivalence("\u{1ECA}", "\u{1ECB}", "C");//C
testCaseEquivalence("\u{1ECC}", "\u{1ECD}", "C");//C
testCaseEquivalence("\u{1ECE}", "\u{1ECF}", "C");//C
testCaseEquivalence("\u{1ED0}", "\u{1ED1}", "C");//C
testCaseEquivalence("\u{1ED2}", "\u{1ED3}", "C");//C
testCaseEquivalence("\u{1ED4}", "\u{1ED5}", "C");//C
testCaseEquivalence("\u{1ED6}", "\u{1ED7}", "C");//C
testCaseEquivalence("\u{1ED8}", "\u{1ED9}", "C");//C
testCaseEquivalence("\u{1EDA}", "\u{1EDB}", "C");//C
testCaseEquivalence("\u{1EDC}", "\u{1EDD}", "C");//C
testCaseEquivalence("\u{1EDE}", "\u{1EDF}", "C");//C
testCaseEquivalence("\u{1EE0}", "\u{1EE1}", "C");//C
testCaseEquivalence("\u{1EE2}", "\u{1EE3}", "C");//C
testCaseEquivalence("\u{1EE4}", "\u{1EE5}", "C");//C
testCaseEquivalence("\u{1EE6}", "\u{1EE7}", "C");//C
testCaseEquivalence("\u{1EE8}", "\u{1EE9}", "C");//C
testCaseEquivalence("\u{1EEA}", "\u{1EEB}", "C");//C
testCaseEquivalence("\u{1EEC}", "\u{1EED}", "C");//C
testCaseEquivalence("\u{1EEE}", "\u{1EEF}", "C");//C
testCaseEquivalence("\u{1EF0}", "\u{1EF1}", "C");//C
testCaseEquivalence("\u{1EF2}", "\u{1EF3}", "C");//C
testCaseEquivalence("\u{1EF4}", "\u{1EF5}", "C");//C
testCaseEquivalence("\u{1EF6}", "\u{1EF7}", "C");//C
testCaseEquivalence("\u{1EF8}", "\u{1EF9}", "C");//C
testCaseEquivalence("\u{1EFA}", "\u{1EFB}", "C");//C
testCaseEquivalence("\u{1EFC}", "\u{1EFD}", "C");//C
testCaseEquivalence("\u{1EFE}", "\u{1EFF}", "C");//C
testCaseEquivalence("\u{1F08}", "\u{1F00}", "C");//C
testCaseEquivalence("\u{1F09}", "\u{1F01}", "C");//C
testCaseEquivalence("\u{1F0A}", "\u{1F02}", "C");//C
testCaseEquivalence("\u{1F0B}", "\u{1F03}", "C");//C
testCaseEquivalence("\u{1F0C}", "\u{1F04}", "C");//C
testCaseEquivalence("\u{1F0D}", "\u{1F05}", "C");//C
testCaseEquivalence("\u{1F0E}", "\u{1F06}", "C");//C
testCaseEquivalence("\u{1F0F}", "\u{1F07}", "C");//C
testCaseEquivalence("\u{1F18}", "\u{1F10}", "C");//C
testCaseEquivalence("\u{1F19}", "\u{1F11}", "C");//C
testCaseEquivalence("\u{1F1A}", "\u{1F12}", "C");//C
testCaseEquivalence("\u{1F1B}", "\u{1F13}", "C");//C
testCaseEquivalence("\u{1F1C}", "\u{1F14}", "C");//C
testCaseEquivalence("\u{1F1D}", "\u{1F15}", "C");//C
testCaseEquivalence("\u{1F28}", "\u{1F20}", "C");//C
testCaseEquivalence("\u{1F29}", "\u{1F21}", "C");//C
testCaseEquivalence("\u{1F2A}", "\u{1F22}", "C");//C
testCaseEquivalence("\u{1F2B}", "\u{1F23}", "C");//C
testCaseEquivalence("\u{1F2C}", "\u{1F24}", "C");//C
testCaseEquivalence("\u{1F2D}", "\u{1F25}", "C");//C
testCaseEquivalence("\u{1F2E}", "\u{1F26}", "C");//C
testCaseEquivalence("\u{1F2F}", "\u{1F27}", "C");//C
testCaseEquivalence("\u{1F38}", "\u{1F30}", "C");//C
testCaseEquivalence("\u{1F39}", "\u{1F31}", "C");//C
testCaseEquivalence("\u{1F3A}", "\u{1F32}", "C");//C
testCaseEquivalence("\u{1F3B}", "\u{1F33}", "C");//C
testCaseEquivalence("\u{1F3C}", "\u{1F34}", "C");//C
testCaseEquivalence("\u{1F3D}", "\u{1F35}", "C");//C
testCaseEquivalence("\u{1F3E}", "\u{1F36}", "C");//C
testCaseEquivalence("\u{1F3F}", "\u{1F37}", "C");//C
testCaseEquivalence("\u{1F48}", "\u{1F40}", "C");//C
testCaseEquivalence("\u{1F49}", "\u{1F41}", "C");//C
testCaseEquivalence("\u{1F4A}", "\u{1F42}", "C");//C
testCaseEquivalence("\u{1F4B}", "\u{1F43}", "C");//C
testCaseEquivalence("\u{1F4C}", "\u{1F44}", "C");//C
testCaseEquivalence("\u{1F4D}", "\u{1F45}", "C");//C
testCaseEquivalence("\u{1F59}", "\u{1F51}", "C");//C
testCaseEquivalence("\u{1F5B}", "\u{1F53}", "C");//C
testCaseEquivalence("\u{1F5D}", "\u{1F55}", "C");//C
testCaseEquivalence("\u{1F5F}", "\u{1F57}", "C");//C
testCaseEquivalence("\u{1F68}", "\u{1F60}", "C");//C
testCaseEquivalence("\u{1F69}", "\u{1F61}", "C");//C
testCaseEquivalence("\u{1F6A}", "\u{1F62}", "C");//C
testCaseEquivalence("\u{1F6B}", "\u{1F63}", "C");//C
testCaseEquivalence("\u{1F6C}", "\u{1F64}", "C");//C
testCaseEquivalence("\u{1F6D}", "\u{1F65}", "C");//C
testCaseEquivalence("\u{1F6E}", "\u{1F66}", "C");//C
testCaseEquivalence("\u{1F6F}", "\u{1F67}", "C");//C
testCaseEquivalence("\u{1F88}", "\u{1F80}", "S");//S
testCaseEquivalence("\u{1F89}", "\u{1F81}", "S");//S
testCaseEquivalence("\u{1F8A}", "\u{1F82}", "S");//S
testCaseEquivalence("\u{1F8B}", "\u{1F83}", "S");//S
testCaseEquivalence("\u{1F8C}", "\u{1F84}", "S");//S
testCaseEquivalence("\u{1F8D}", "\u{1F85}", "S");//S
testCaseEquivalence("\u{1F8E}", "\u{1F86}", "S");//S
testCaseEquivalence("\u{1F8F}", "\u{1F87}", "S");//S
testCaseEquivalence("\u{1F98}", "\u{1F90}", "S");//S
testCaseEquivalence("\u{1F99}", "\u{1F91}", "S");//S
testCaseEquivalence("\u{1F9A}", "\u{1F92}", "S");//S
testCaseEquivalence("\u{1F9B}", "\u{1F93}", "S");//S
testCaseEquivalence("\u{1F9C}", "\u{1F94}", "S");//S
testCaseEquivalence("\u{1F9D}", "\u{1F95}", "S");//S
testCaseEquivalence("\u{1F9E}", "\u{1F96}", "S");//S
testCaseEquivalence("\u{1F9F}", "\u{1F97}", "S");//S
testCaseEquivalence("\u{1FA8}", "\u{1FA0}", "S");//S
testCaseEquivalence("\u{1FA9}", "\u{1FA1}", "S");//S
testCaseEquivalence("\u{1FAA}", "\u{1FA2}", "S");//S
testCaseEquivalence("\u{1FAB}", "\u{1FA3}", "S");//S
testCaseEquivalence("\u{1FAC}", "\u{1FA4}", "S");//S
testCaseEquivalence("\u{1FAD}", "\u{1FA5}", "S");//S
testCaseEquivalence("\u{1FAE}", "\u{1FA6}", "S");//S
testCaseEquivalence("\u{1FAF}", "\u{1FA7}", "S");//S
testCaseEquivalence("\u{1FB8}", "\u{1FB0}", "C");//C
testCaseEquivalence("\u{1FB9}", "\u{1FB1}", "C");//C
testCaseEquivalence("\u{1FBA}", "\u{1F70}", "C");//C
testCaseEquivalence("\u{1FBB}", "\u{1F71}", "C");//C
testCaseEquivalence("\u{1FBC}", "\u{1FB3}", "S");//S
testCaseEquivalence("\u{1FBE}", "\u{03B9}", "C");//C
testCaseEquivalence("\u{1FC8}", "\u{1F72}", "C");//C
testCaseEquivalence("\u{1FC9}", "\u{1F73}", "C");//C
testCaseEquivalence("\u{1FCA}", "\u{1F74}", "C");//C
testCaseEquivalence("\u{1FCB}", "\u{1F75}", "C");//C
testCaseEquivalence("\u{1FCC}", "\u{1FC3}", "S");//S
testCaseEquivalence("\u{1FD8}", "\u{1FD0}", "C");//C
testCaseEquivalence("\u{1FD9}", "\u{1FD1}", "C");//C
testCaseEquivalence("\u{1FDA}", "\u{1F76}", "C");//C
testCaseEquivalence("\u{1FDB}", "\u{1F77}", "C");//C
testCaseEquivalence("\u{1FE8}", "\u{1FE0}", "C");//C
testCaseEquivalence("\u{1FE9}", "\u{1FE1}", "C");//C
testCaseEquivalence("\u{1FEA}", "\u{1F7A}", "C");//C
testCaseEquivalence("\u{1FEB}", "\u{1F7B}", "C");//C
testCaseEquivalence("\u{1FEC}", "\u{1FE5}", "C");//C
testCaseEquivalence("\u{1FF8}", "\u{1F78}", "C");//C
testCaseEquivalence("\u{1FF9}", "\u{1F79}", "C");//C
testCaseEquivalence("\u{1FFA}", "\u{1F7C}", "C");//C
testCaseEquivalence("\u{1FFB}", "\u{1F7D}", "C");//C
testCaseEquivalence("\u{1FFC}", "\u{1FF3}", "S");//S
testCaseEquivalence("\u{2126}", "\u{03C9}", "C");//C
testCaseEquivalence("\u{212A}", "\u{006B}", "C");//C
testCaseEquivalence("\u{212B}", "\u{00E5}", "C");//C
testCaseEquivalence("\u{2132}", "\u{214E}", "C");//C
testCaseEquivalence("\u{2160}", "\u{2170}", "C");//C
testCaseEquivalence("\u{2161}", "\u{2171}", "C");//C
testCaseEquivalence("\u{2162}", "\u{2172}", "C");//C
testCaseEquivalence("\u{2163}", "\u{2173}", "C");//C
testCaseEquivalence("\u{2164}", "\u{2174}", "C");//C
testCaseEquivalence("\u{2165}", "\u{2175}", "C");//C
testCaseEquivalence("\u{2166}", "\u{2176}", "C");//C
testCaseEquivalence("\u{2167}", "\u{2177}", "C");//C
testCaseEquivalence("\u{2168}", "\u{2178}", "C");//C
testCaseEquivalence("\u{2169}", "\u{2179}", "C");//C
testCaseEquivalence("\u{216A}", "\u{217A}", "C");//C
testCaseEquivalence("\u{216B}", "\u{217B}", "C");//C
testCaseEquivalence("\u{216C}", "\u{217C}", "C");//C
testCaseEquivalence("\u{216D}", "\u{217D}", "C");//C
testCaseEquivalence("\u{216E}", "\u{217E}", "C");//C
testCaseEquivalence("\u{216F}", "\u{217F}", "C");//C
testCaseEquivalence("\u{2183}", "\u{2184}", "C");//C
testCaseEquivalence("\u{24B6}", "\u{24D0}", "C");//C
testCaseEquivalence("\u{24B7}", "\u{24D1}", "C");//C
testCaseEquivalence("\u{24B8}", "\u{24D2}", "C");//C
testCaseEquivalence("\u{24B9}", "\u{24D3}", "C");//C
testCaseEquivalence("\u{24BA}", "\u{24D4}", "C");//C
testCaseEquivalence("\u{24BB}", "\u{24D5}", "C");//C
testCaseEquivalence("\u{24BC}", "\u{24D6}", "C");//C
testCaseEquivalence("\u{24BD}", "\u{24D7}", "C");//C
testCaseEquivalence("\u{24BE}", "\u{24D8}", "C");//C
testCaseEquivalence("\u{24BF}", "\u{24D9}", "C");//C
testCaseEquivalence("\u{24C0}", "\u{24DA}", "C");//C
testCaseEquivalence("\u{24C1}", "\u{24DB}", "C");//C
testCaseEquivalence("\u{24C2}", "\u{24DC}", "C");//C
testCaseEquivalence("\u{24C3}", "\u{24DD}", "C");//C
testCaseEquivalence("\u{24C4}", "\u{24DE}", "C");//C
testCaseEquivalence("\u{24C5}", "\u{24DF}", "C");//C
testCaseEquivalence("\u{24C6}", "\u{24E0}", "C");//C
testCaseEquivalence("\u{24C7}", "\u{24E1}", "C");//C
testCaseEquivalence("\u{24C8}", "\u{24E2}", "C");//C
testCaseEquivalence("\u{24C9}", "\u{24E3}", "C");//C
testCaseEquivalence("\u{24CA}", "\u{24E4}", "C");//C
testCaseEquivalence("\u{24CB}", "\u{24E5}", "C");//C
testCaseEquivalence("\u{24CC}", "\u{24E6}", "C");//C
testCaseEquivalence("\u{24CD}", "\u{24E7}", "C");//C
testCaseEquivalence("\u{24CE}", "\u{24E8}", "C");//C
testCaseEquivalence("\u{24CF}", "\u{24E9}", "C");//C
testCaseEquivalence("\u{2C00}", "\u{2C30}", "C");//C
testCaseEquivalence("\u{2C01}", "\u{2C31}", "C");//C
testCaseEquivalence("\u{2C02}", "\u{2C32}", "C");//C
testCaseEquivalence("\u{2C03}", "\u{2C33}", "C");//C
testCaseEquivalence("\u{2C04}", "\u{2C34}", "C");//C
testCaseEquivalence("\u{2C05}", "\u{2C35}", "C");//C
testCaseEquivalence("\u{2C06}", "\u{2C36}", "C");//C
testCaseEquivalence("\u{2C07}", "\u{2C37}", "C");//C
testCaseEquivalence("\u{2C08}", "\u{2C38}", "C");//C
testCaseEquivalence("\u{2C09}", "\u{2C39}", "C");//C
testCaseEquivalence("\u{2C0A}", "\u{2C3A}", "C");//C
testCaseEquivalence("\u{2C0B}", "\u{2C3B}", "C");//C
testCaseEquivalence("\u{2C0C}", "\u{2C3C}", "C");//C
testCaseEquivalence("\u{2C0D}", "\u{2C3D}", "C");//C
testCaseEquivalence("\u{2C0E}", "\u{2C3E}", "C");//C
testCaseEquivalence("\u{2C0F}", "\u{2C3F}", "C");//C
testCaseEquivalence("\u{2C10}", "\u{2C40}", "C");//C
testCaseEquivalence("\u{2C11}", "\u{2C41}", "C");//C
testCaseEquivalence("\u{2C12}", "\u{2C42}", "C");//C
testCaseEquivalence("\u{2C13}", "\u{2C43}", "C");//C
testCaseEquivalence("\u{2C14}", "\u{2C44}", "C");//C
testCaseEquivalence("\u{2C15}", "\u{2C45}", "C");//C
testCaseEquivalence("\u{2C16}", "\u{2C46}", "C");//C
testCaseEquivalence("\u{2C17}", "\u{2C47}", "C");//C
testCaseEquivalence("\u{2C18}", "\u{2C48}", "C");//C
testCaseEquivalence("\u{2C19}", "\u{2C49}", "C");//C
testCaseEquivalence("\u{2C1A}", "\u{2C4A}", "C");//C
testCaseEquivalence("\u{2C1B}", "\u{2C4B}", "C");//C
testCaseEquivalence("\u{2C1C}", "\u{2C4C}", "C");//C
testCaseEquivalence("\u{2C1D}", "\u{2C4D}", "C");//C
testCaseEquivalence("\u{2C1E}", "\u{2C4E}", "C");//C
testCaseEquivalence("\u{2C1F}", "\u{2C4F}", "C");//C
testCaseEquivalence("\u{2C20}", "\u{2C50}", "C");//C
testCaseEquivalence("\u{2C21}", "\u{2C51}", "C");//C
testCaseEquivalence("\u{2C22}", "\u{2C52}", "C");//C
testCaseEquivalence("\u{2C23}", "\u{2C53}", "C");//C
testCaseEquivalence("\u{2C24}", "\u{2C54}", "C");//C
testCaseEquivalence("\u{2C25}", "\u{2C55}", "C");//C
testCaseEquivalence("\u{2C26}", "\u{2C56}", "C");//C
testCaseEquivalence("\u{2C27}", "\u{2C57}", "C");//C
testCaseEquivalence("\u{2C28}", "\u{2C58}", "C");//C
testCaseEquivalence("\u{2C29}", "\u{2C59}", "C");//C
testCaseEquivalence("\u{2C2A}", "\u{2C5A}", "C");//C
testCaseEquivalence("\u{2C2B}", "\u{2C5B}", "C");//C
testCaseEquivalence("\u{2C2C}", "\u{2C5C}", "C");//C
testCaseEquivalence("\u{2C2D}", "\u{2C5D}", "C");//C
testCaseEquivalence("\u{2C2E}", "\u{2C5E}", "C");//C
testCaseEquivalence("\u{2C60}", "\u{2C61}", "C");//C
testCaseEquivalence("\u{2C62}", "\u{026B}", "C");//C
testCaseEquivalence("\u{2C63}", "\u{1D7D}", "C");//C
testCaseEquivalence("\u{2C64}", "\u{027D}", "C");//C
testCaseEquivalence("\u{2C67}", "\u{2C68}", "C");//C
testCaseEquivalence("\u{2C69}", "\u{2C6A}", "C");//C
testCaseEquivalence("\u{2C6B}", "\u{2C6C}", "C");//C
testCaseEquivalence("\u{2C6D}", "\u{0251}", "C");//C
testCaseEquivalence("\u{2C6E}", "\u{0271}", "C");//C
testCaseEquivalence("\u{2C6F}", "\u{0250}", "C");//C
testCaseEquivalence("\u{2C70}", "\u{0252}", "C");//C
testCaseEquivalence("\u{2C72}", "\u{2C73}", "C");//C
testCaseEquivalence("\u{2C75}", "\u{2C76}", "C");//C
testCaseEquivalence("\u{2C7E}", "\u{023F}", "C");//C
testCaseEquivalence("\u{2C7F}", "\u{0240}", "C");//C
testCaseEquivalence("\u{2C80}", "\u{2C81}", "C");//C
testCaseEquivalence("\u{2C82}", "\u{2C83}", "C");//C
testCaseEquivalence("\u{2C84}", "\u{2C85}", "C");//C
testCaseEquivalence("\u{2C86}", "\u{2C87}", "C");//C
testCaseEquivalence("\u{2C88}", "\u{2C89}", "C");//C
testCaseEquivalence("\u{2C8A}", "\u{2C8B}", "C");//C
testCaseEquivalence("\u{2C8C}", "\u{2C8D}", "C");//C
testCaseEquivalence("\u{2C8E}", "\u{2C8F}", "C");//C
testCaseEquivalence("\u{2C90}", "\u{2C91}", "C");//C
testCaseEquivalence("\u{2C92}", "\u{2C93}", "C");//C
testCaseEquivalence("\u{2C94}", "\u{2C95}", "C");//C
testCaseEquivalence("\u{2C96}", "\u{2C97}", "C");//C
testCaseEquivalence("\u{2C98}", "\u{2C99}", "C");//C
testCaseEquivalence("\u{2C9A}", "\u{2C9B}", "C");//C
testCaseEquivalence("\u{2C9C}", "\u{2C9D}", "C");//C
testCaseEquivalence("\u{2C9E}", "\u{2C9F}", "C");//C
testCaseEquivalence("\u{2CA0}", "\u{2CA1}", "C");//C
testCaseEquivalence("\u{2CA2}", "\u{2CA3}", "C");//C
testCaseEquivalence("\u{2CA4}", "\u{2CA5}", "C");//C
testCaseEquivalence("\u{2CA6}", "\u{2CA7}", "C");//C
testCaseEquivalence("\u{2CA8}", "\u{2CA9}", "C");//C
testCaseEquivalence("\u{2CAA}", "\u{2CAB}", "C");//C
testCaseEquivalence("\u{2CAC}", "\u{2CAD}", "C");//C
testCaseEquivalence("\u{2CAE}", "\u{2CAF}", "C");//C
testCaseEquivalence("\u{2CB0}", "\u{2CB1}", "C");//C
testCaseEquivalence("\u{2CB2}", "\u{2CB3}", "C");//C
testCaseEquivalence("\u{2CB4}", "\u{2CB5}", "C");//C
testCaseEquivalence("\u{2CB6}", "\u{2CB7}", "C");//C
testCaseEquivalence("\u{2CB8}", "\u{2CB9}", "C");//C
testCaseEquivalence("\u{2CBA}", "\u{2CBB}", "C");//C
testCaseEquivalence("\u{2CBC}", "\u{2CBD}", "C");//C
testCaseEquivalence("\u{2CBE}", "\u{2CBF}", "C");//C
testCaseEquivalence("\u{2CC0}", "\u{2CC1}", "C");//C
testCaseEquivalence("\u{2CC2}", "\u{2CC3}", "C");//C
testCaseEquivalence("\u{2CC4}", "\u{2CC5}", "C");//C
testCaseEquivalence("\u{2CC6}", "\u{2CC7}", "C");//C
testCaseEquivalence("\u{2CC8}", "\u{2CC9}", "C");//C
testCaseEquivalence("\u{2CCA}", "\u{2CCB}", "C");//C
testCaseEquivalence("\u{2CCC}", "\u{2CCD}", "C");//C
testCaseEquivalence("\u{2CCE}", "\u{2CCF}", "C");//C
testCaseEquivalence("\u{2CD0}", "\u{2CD1}", "C");//C
testCaseEquivalence("\u{2CD2}", "\u{2CD3}", "C");//C
testCaseEquivalence("\u{2CD4}", "\u{2CD5}", "C");//C
testCaseEquivalence("\u{2CD6}", "\u{2CD7}", "C");//C
testCaseEquivalence("\u{2CD8}", "\u{2CD9}", "C");//C
testCaseEquivalence("\u{2CDA}", "\u{2CDB}", "C");//C
testCaseEquivalence("\u{2CDC}", "\u{2CDD}", "C");//C
testCaseEquivalence("\u{2CDE}", "\u{2CDF}", "C");//C
testCaseEquivalence("\u{2CE0}", "\u{2CE1}", "C");//C
testCaseEquivalence("\u{2CE2}", "\u{2CE3}", "C");//C
testCaseEquivalence("\u{2CEB}", "\u{2CEC}", "C");//C
testCaseEquivalence("\u{2CED}", "\u{2CEE}", "C");//C
testCaseEquivalence("\u{2CF2}", "\u{2CF3}", "C");//C
testCaseEquivalence("\u{A640}", "\u{A641}", "C");//C
testCaseEquivalence("\u{A642}", "\u{A643}", "C");//C
testCaseEquivalence("\u{A644}", "\u{A645}", "C");//C
testCaseEquivalence("\u{A646}", "\u{A647}", "C");//C
testCaseEquivalence("\u{A648}", "\u{A649}", "C");//C
testCaseEquivalence("\u{A64A}", "\u{A64B}", "C");//C
testCaseEquivalence("\u{A64C}", "\u{A64D}", "C");//C
testCaseEquivalence("\u{A64E}", "\u{A64F}", "C");//C
testCaseEquivalence("\u{A650}", "\u{A651}", "C");//C
testCaseEquivalence("\u{A652}", "\u{A653}", "C");//C
testCaseEquivalence("\u{A654}", "\u{A655}", "C");//C
testCaseEquivalence("\u{A656}", "\u{A657}", "C");//C
testCaseEquivalence("\u{A658}", "\u{A659}", "C");//C
testCaseEquivalence("\u{A65A}", "\u{A65B}", "C");//C
testCaseEquivalence("\u{A65C}", "\u{A65D}", "C");//C
testCaseEquivalence("\u{A65E}", "\u{A65F}", "C");//C
testCaseEquivalence("\u{A660}", "\u{A661}", "C");//C
testCaseEquivalence("\u{A662}", "\u{A663}", "C");//C
testCaseEquivalence("\u{A664}", "\u{A665}", "C");//C
testCaseEquivalence("\u{A666}", "\u{A667}", "C");//C
testCaseEquivalence("\u{A668}", "\u{A669}", "C");//C
testCaseEquivalence("\u{A66A}", "\u{A66B}", "C");//C
testCaseEquivalence("\u{A66C}", "\u{A66D}", "C");//C
testCaseEquivalence("\u{A680}", "\u{A681}", "C");//C
testCaseEquivalence("\u{A682}", "\u{A683}", "C");//C
testCaseEquivalence("\u{A684}", "\u{A685}", "C");//C
testCaseEquivalence("\u{A686}", "\u{A687}", "C");//C
testCaseEquivalence("\u{A688}", "\u{A689}", "C");//C
testCaseEquivalence("\u{A68A}", "\u{A68B}", "C");//C
testCaseEquivalence("\u{A68C}", "\u{A68D}", "C");//C
testCaseEquivalence("\u{A68E}", "\u{A68F}", "C");//C
testCaseEquivalence("\u{A690}", "\u{A691}", "C");//C
testCaseEquivalence("\u{A692}", "\u{A693}", "C");//C
testCaseEquivalence("\u{A694}", "\u{A695}", "C");//C
testCaseEquivalence("\u{A696}", "\u{A697}", "C");//C
testCaseEquivalence("\u{A722}", "\u{A723}", "C");//C
testCaseEquivalence("\u{A724}", "\u{A725}", "C");//C
testCaseEquivalence("\u{A726}", "\u{A727}", "C");//C
testCaseEquivalence("\u{A728}", "\u{A729}", "C");//C
testCaseEquivalence("\u{A72A}", "\u{A72B}", "C");//C
testCaseEquivalence("\u{A72C}", "\u{A72D}", "C");//C
testCaseEquivalence("\u{A72E}", "\u{A72F}", "C");//C
testCaseEquivalence("\u{A732}", "\u{A733}", "C");//C
testCaseEquivalence("\u{A734}", "\u{A735}", "C");//C
testCaseEquivalence("\u{A736}", "\u{A737}", "C");//C
testCaseEquivalence("\u{A738}", "\u{A739}", "C");//C
testCaseEquivalence("\u{A73A}", "\u{A73B}", "C");//C
testCaseEquivalence("\u{A73C}", "\u{A73D}", "C");//C
testCaseEquivalence("\u{A73E}", "\u{A73F}", "C");//C
testCaseEquivalence("\u{A740}", "\u{A741}", "C");//C
testCaseEquivalence("\u{A742}", "\u{A743}", "C");//C
testCaseEquivalence("\u{A744}", "\u{A745}", "C");//C
testCaseEquivalence("\u{A746}", "\u{A747}", "C");//C
testCaseEquivalence("\u{A748}", "\u{A749}", "C");//C
testCaseEquivalence("\u{A74A}", "\u{A74B}", "C");//C
testCaseEquivalence("\u{A74C}", "\u{A74D}", "C");//C
testCaseEquivalence("\u{A74E}", "\u{A74F}", "C");//C
testCaseEquivalence("\u{A750}", "\u{A751}", "C");//C
testCaseEquivalence("\u{A752}", "\u{A753}", "C");//C
testCaseEquivalence("\u{A754}", "\u{A755}", "C");//C
testCaseEquivalence("\u{A756}", "\u{A757}", "C");//C
testCaseEquivalence("\u{A758}", "\u{A759}", "C");//C
testCaseEquivalence("\u{A75A}", "\u{A75B}", "C");//C
testCaseEquivalence("\u{A75C}", "\u{A75D}", "C");//C
testCaseEquivalence("\u{A75E}", "\u{A75F}", "C");//C
testCaseEquivalence("\u{A760}", "\u{A761}", "C");//C
testCaseEquivalence("\u{A762}", "\u{A763}", "C");//C
testCaseEquivalence("\u{A764}", "\u{A765}", "C");//C
testCaseEquivalence("\u{A766}", "\u{A767}", "C");//C
testCaseEquivalence("\u{A768}", "\u{A769}", "C");//C
testCaseEquivalence("\u{A76A}", "\u{A76B}", "C");//C
testCaseEquivalence("\u{A76C}", "\u{A76D}", "C");//C
testCaseEquivalence("\u{A76E}", "\u{A76F}", "C");//C
testCaseEquivalence("\u{A779}", "\u{A77A}", "C");//C
testCaseEquivalence("\u{A77B}", "\u{A77C}", "C");//C
testCaseEquivalence("\u{A77D}", "\u{1D79}", "C");//C
testCaseEquivalence("\u{A77E}", "\u{A77F}", "C");//C
testCaseEquivalence("\u{A780}", "\u{A781}", "C");//C
testCaseEquivalence("\u{A782}", "\u{A783}", "C");//C
testCaseEquivalence("\u{A784}", "\u{A785}", "C");//C
testCaseEquivalence("\u{A786}", "\u{A787}", "C");//C
testCaseEquivalence("\u{A78B}", "\u{A78C}", "C");//C
testCaseEquivalence("\u{A78D}", "\u{0265}", "C");//C
testCaseEquivalence("\u{A790}", "\u{A791}", "C");//C
testCaseEquivalence("\u{A792}", "\u{A793}", "C");//C
testCaseEquivalence("\u{A7A0}", "\u{A7A1}", "C");//C
testCaseEquivalence("\u{A7A2}", "\u{A7A3}", "C");//C
testCaseEquivalence("\u{A7A4}", "\u{A7A5}", "C");//C
testCaseEquivalence("\u{A7A6}", "\u{A7A7}", "C");//C
testCaseEquivalence("\u{A7A8}", "\u{A7A9}", "C");//C
testCaseEquivalence("\u{A7AA}", "\u{0266}", "C");//C
testCaseEquivalence("\u{FF21}", "\u{FF41}", "C");//C
testCaseEquivalence("\u{FF22}", "\u{FF42}", "C");//C
testCaseEquivalence("\u{FF23}", "\u{FF43}", "C");//C
testCaseEquivalence("\u{FF24}", "\u{FF44}", "C");//C
testCaseEquivalence("\u{FF25}", "\u{FF45}", "C");//C
testCaseEquivalence("\u{FF26}", "\u{FF46}", "C");//C
testCaseEquivalence("\u{FF27}", "\u{FF47}", "C");//C
testCaseEquivalence("\u{FF28}", "\u{FF48}", "C");//C
testCaseEquivalence("\u{FF29}", "\u{FF49}", "C");//C
testCaseEquivalence("\u{FF2A}", "\u{FF4A}", "C");//C
testCaseEquivalence("\u{FF2B}", "\u{FF4B}", "C");//C
testCaseEquivalence("\u{FF2C}", "\u{FF4C}", "C");//C
testCaseEquivalence("\u{FF2D}", "\u{FF4D}", "C");//C
testCaseEquivalence("\u{FF2E}", "\u{FF4E}", "C");//C
testCaseEquivalence("\u{FF2F}", "\u{FF4F}", "C");//C
testCaseEquivalence("\u{FF30}", "\u{FF50}", "C");//C
testCaseEquivalence("\u{FF31}", "\u{FF51}", "C");//C
testCaseEquivalence("\u{FF32}", "\u{FF52}", "C");//C
testCaseEquivalence("\u{FF33}", "\u{FF53}", "C");//C
testCaseEquivalence("\u{FF34}", "\u{FF54}", "C");//C
testCaseEquivalence("\u{FF35}", "\u{FF55}", "C");//C
testCaseEquivalence("\u{FF36}", "\u{FF56}", "C");//C
testCaseEquivalence("\u{FF37}", "\u{FF57}", "C");//C
testCaseEquivalence("\u{FF38}", "\u{FF58}", "C");//C
testCaseEquivalence("\u{FF39}", "\u{FF59}", "C");//C
testCaseEquivalence("\u{FF3A}", "\u{FF5A}", "C");//C
testCaseEquivalence("\u{10400}", "\u{10428}", "C");//C
testCaseEquivalence("\u{10401}", "\u{10429}", "C");//C
testCaseEquivalence("\u{10402}", "\u{1042A}", "C");//C
testCaseEquivalence("\u{10403}", "\u{1042B}", "C");//C
testCaseEquivalence("\u{10404}", "\u{1042C}", "C");//C
testCaseEquivalence("\u{10405}", "\u{1042D}", "C");//C
testCaseEquivalence("\u{10406}", "\u{1042E}", "C");//C
testCaseEquivalence("\u{10407}", "\u{1042F}", "C");//C
testCaseEquivalence("\u{10408}", "\u{10430}", "C");//C
testCaseEquivalence("\u{10409}", "\u{10431}", "C");//C
testCaseEquivalence("\u{1040A}", "\u{10432}", "C");//C
testCaseEquivalence("\u{1040B}", "\u{10433}", "C");//C
testCaseEquivalence("\u{1040C}", "\u{10434}", "C");//C
testCaseEquivalence("\u{1040D}", "\u{10435}", "C");//C
testCaseEquivalence("\u{1040E}", "\u{10436}", "C");//C
testCaseEquivalence("\u{1040F}", "\u{10437}", "C");//C
testCaseEquivalence("\u{10410}", "\u{10438}", "C");//C
testCaseEquivalence("\u{10411}", "\u{10439}", "C");//C
testCaseEquivalence("\u{10412}", "\u{1043A}", "C");//C
testCaseEquivalence("\u{10413}", "\u{1043B}", "C");//C
testCaseEquivalence("\u{10414}", "\u{1043C}", "C");//C
testCaseEquivalence("\u{10415}", "\u{1043D}", "C");//C
testCaseEquivalence("\u{10416}", "\u{1043E}", "C");//C
testCaseEquivalence("\u{10417}", "\u{1043F}", "C");//C
testCaseEquivalence("\u{10418}", "\u{10440}", "C");//C
testCaseEquivalence("\u{10419}", "\u{10441}", "C");//C
testCaseEquivalence("\u{1041A}", "\u{10442}", "C");//C
testCaseEquivalence("\u{1041B}", "\u{10443}", "C");//C
testCaseEquivalence("\u{1041C}", "\u{10444}", "C");//C
testCaseEquivalence("\u{1041D}", "\u{10445}", "C");//C
testCaseEquivalence("\u{1041E}", "\u{10446}", "C");//C
testCaseEquivalence("\u{1041F}", "\u{10447}", "C");//C
testCaseEquivalence("\u{10420}", "\u{10448}", "C");//C
testCaseEquivalence("\u{10421}", "\u{10449}", "C");//C
testCaseEquivalence("\u{10422}", "\u{1044A}", "C");//C
testCaseEquivalence("\u{10423}", "\u{1044B}", "C");//C
testCaseEquivalence("\u{10424}", "\u{1044C}", "C");//C
testCaseEquivalence("\u{10425}", "\u{1044D}", "C");//C
testCaseEquivalence("\u{10426}", "\u{1044E}", "C");//C
testCaseEquivalence("\u{10427}", "\u{1044F}", "C");//C

("Passed: " + (totalTests - numFailures) + "/" + totalTests).echo();

"============ 4. Random tests ============".echo();

/(a|d|q|)x/i.exec("bcaDxqy").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10000}x").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10400}x").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10429}x").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10401}x").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10428}x").echo();
/[a-z\u{10000}-\u{15000}]/iu.test("A").echo();
/[a-z\u{10400}-\u{10427}]/ui.test("\u{10428}").echo();
/\u{10400}/ui.test("\u{10428}").echo();
/[a-z\u{10400}-\u{10427}]/ui.test("\u{1044f}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10428}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10429}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{1042A}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{1042B}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{1042C}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{1042D}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{1042E}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{1042F}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10430}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10431}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10432}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10433}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10434}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10435}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10436}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10437}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("Z").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("A").echo();
/[aaaaa]/ui.test("A").echo();
/[aaaaa]/u.test("a").echo();
/[\u{10400}\u{10400}\u{10400}]/u.test("\u{10400}").echo();
/[\u{10400}\u{10400}\u{10400}]/ui.test("\u{10428}").echo();
/[^\u{10400}]/ui.test("A").echo();
/[^\u{10400}]/ui.test("\u{10401}").echo();
//Testing to make sure that negation doesn't overflow to Surrogate plane.
/a[^.]b/i.test("a\ud800b").echo();

//Verifing that negation does overflow to surrogate plane.
/a[^.]b/ui.test("a\ud800\udc00b").echo();
/[\u017a-\u017d]/i.test("\u017e").echo();

// Test '-' as end of range, with one more char after it.
/[)-- ]/.test(")").echo();

/^.$/u.test('\u{1D306}').echo();
/^\S$/u.test("\u{1D306}").echo();
/^\W$/u.test("\u{1D306}").echo();
/^\D$/u.test("\u{1D306}").echo();

"===============================================".echo();

/[a-z\u{10400}-\u{10427}]/ui.test("\u{10450}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10427}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{103E8}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{103E9}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u{10438}").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u005B").echo();
/[a-z\u{10400}\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040A}\u{1040B}\u{1040C}\u{1040D}\u{1040E}\u{1040F}]/ui.test("\u0040").echo();
/[aaaaa]/u.test("A").echo();
/[aaaaa]/u.test("b").echo();
/[\u{10400}\u{10400}\u{10400}]/u.test("\u{10401}").echo();
/[\u{10400}\u{10400}\u{10400}]/u.test("\u{10428}").echo();
/a[^\u{10400}]b/ui.test("a\u{10400}b").echo();
/a[^\u{10400}]b/ui.test("a\u{10428}b").echo();
/a[\u0000-\u{20BB7}]b/u.test("a\u{20BB8}b").echo();
/a[^.]b/i.test("a\ud800\udc00b").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\uD800x").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\uD800x").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\uD800x").echo();
/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\uD800x").echo();
/^.$/.test("\u{1D306}").echo();
/^\S$/.test("\u{1D306}").echo();
/^\W$/.test("\u{1D306}").echo();
/^\D$/.test("\u{1D306}").echo();

try{
    new RegExp("[b-G\\D]").exec("a")
} catch(ex){
    ex.echo();
}

var repro = /\2|;*?$6(?!#)FP|&{1,4}|(?=R)i??|(?!(?=h|[^ᆉ-䓈ѫ-񢊋]|\x0Fe{0,2}|\1)?d)/iu
repro = /\3*?|(?=W)|I|[槌-󽆺]+((?="*)){0,2}?|>|(?:(?=1){3,}|꣏?){4}/gimu
repro = /a[\u{20BB7}]b/u;
