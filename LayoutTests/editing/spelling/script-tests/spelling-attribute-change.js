description('Tests if the spellchecker behavior change after the spellcheck attribute changed by the script.');

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
    shouldBe("internals.hasSpellingMarker(document, 6, 2)", enabled ? "true" : "false");
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
    parent.innerHTML = "";
}

// default -> true
testFor("SPAN", undefined, true);
// default -> false
testFor("SPAN", undefined, false);
// true -> true
testFor("SPAN", true, true);
// true -> false
testFor("SPAN", true, false);
// false -> true
testFor("SPAN", false, true);
// false -> false
testFor("SPAN", false, false);

// default -> true
testFor("INPUT", undefined, true);
// default -> false
testFor("INPUT", undefined, false);
// true -> true
testFor("INPUT", true, true);
// true -> false
testFor("INPUT", true, false);
// false -> true
testFor("INPUT", false, true);
// false -> false
testFor("INPUT", false, false);

// default -> true
testFor("TEXTAREA", undefined, true);
// default -> false
testFor("TEXTAREA", undefined, false);
// true -> true
testFor("TEXTAREA", true, true);
// true -> false
testFor("TEXTAREA", true, false);
// false -> true
testFor("TEXTAREA", false, true);
// false -> false
testFor("TEXTAREA", false, false);

var successfullyParsed = true;
