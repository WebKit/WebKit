description(
"Bug 7445, bug 7253: Handle Unicode escapes in regexps."
);

var I3=/^\s*(fwd|re|aw|antw|antwort|wg|sv|ang|odp|betreff|betr|transf|reenv\.|reenv|in|res|resp|resp\.|enc|\u8f6c\u53d1|\u56DE\u590D|\u041F\u0435\u0440\u0435\u0441\u043B|\u041E\u0442\u0432\u0435\u0442):\s*(.*)$/i;

// Other RegExs from Gmail source
var Ci=/\s+/g;
var BC=/^ /;
var BG=/ $/;

// Strips leading Re or similar (from Gmail source)
function cy(a) {
    //var b = I3.exec(a);
    var b = I3.exec(a);

    if (b) {
        a = b[2];
    }

    return Gn(a);
}

// This function replaces consecutive whitespace with a single space
// then removes a leading and trailing space if they exist. (From Gmail)
function Gn(a) {
    return a.replace(Ci, " ").replace(BC, "").replace(BG, "");
}

shouldBe('cy("Re: Hello")', '"Hello"');
shouldBe('cy("Ответ: Hello")', '"Hello"');

// ---------------------------------------------------------------

var regex = /^([^#<\u2264]+)([#<\u2264])(.*)$/;
shouldBe('regex.exec("24#Midnight").toString()', '"24#Midnight,24,#,Midnight"');

var successfullyParsed = true;
