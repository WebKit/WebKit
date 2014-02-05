description('Tests if the spellchecker behavior change after the spellcheck attribute changed by the script.');

jsTestIsAsync = true;

if (window.internals) {
    internals.settings.setUnifiedTextCheckerEnabled(true);
    internals.settings.setAsynchronousSpellCheckingEnabled(true);
}

var parent = document.createElement("div");
document.body.appendChild(parent);
var sel = document.getSelection();

function testSpellCheckingEnabled(target, enabled)
{
    target.spellcheck = enabled;

    if (target.tagName == "SPAN") {
        target.appendChild(document.createTextNode("Hello,"));
        sel.setBaseAndExtent(target, 6, target, 6);
    } else if (target.tagName == "INPUT" || target.tagName == "TEXTAREA") {
        target.focus();
        document.execCommand("InsertText", false, "Hello,");
    }

    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, ' ');

    window.target = target;
    shouldBe("target.spellcheck", enabled ? "true" : "false");
    shouldBecomeEqual("internals.hasSpellingMarker(6, 2)", enabled ? "true" : "false", done);
}

function createElement(tagName, spellcheck)
{
    var target = document.createElement(tagName);
    if (tagName == "SPAN")
        target.setAttribute("contentEditable", "true");
    if (spellcheck)
        target.setAttribute("spellcheck", spellcheck);
    return target;
}

function testFor(tagName, initialAttribute, expectation)
{
    var target = createElement(tagName, initialAttribute);
    parent.appendChild(target);

    testSpellCheckingEnabled(target, expectation);
}

var tests = [ function() { testFor("SPAN", undefined, true); }, // default -> true
              function() { testFor("SPAN", undefined, false); }, // default -> false
              function() { testFor("SPAN", true, true); }, // true -> true
              function() { testFor("SPAN", true, false); }, // true -> false
              function() { testFor("SPAN", false, true); }, // false -> true
              function() { testFor("SPAN", false, false); }, // false -> false

              function() { testFor("INPUT", undefined, true); }, // default -> true
              function() { testFor("INPUT", undefined, false); }, // default -> false
              function() { testFor("INPUT", true, true); }, // true -> true
              function() { testFor("INPUT", true, false); }, // true -> false
              function() { testFor("INPUT", false, true); }, // false -> true
              function() { testFor("INPUT", false, false); }, // false -> false

              function() { testFor("TEXTAREA", undefined, true); }, // default -> true
              function() { testFor("TEXTAREA", undefined, false); }, // default -> false
              function() { testFor("TEXTAREA", true, true); }, // true -> true
              function() { testFor("TEXTAREA", true, false); }, // true -> false
              function() { testFor("TEXTAREA", false, true); }, // false -> true
              function() { testFor("TEXTAREA", false, false); }, // false -> false
            ];

function done()
{
    if (window.internals) {
        var next = tests.shift();
        if (next)
            return window.setTimeout(next, 0);

        parent.innerHTML = "";
    }
    finishJSTest();
}
done();

var successfullyParsed = true;
