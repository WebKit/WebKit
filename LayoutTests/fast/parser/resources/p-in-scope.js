function testAsChild(tag)
{
    var markup = "<p>1<" + tag + ">";
    var container = document.createElement("body");

    container.innerHTML = markup;

    if (container.childNodes.length == 1)
        return "allowed";

    if (container.lastChild.tagName.toLowerCase() == tag)
        return "closed";

    return "other";
}

function testAsGrandchild(tag, intermediateTag)
{
    var markup = "<p>1<" + intermediateTag + ">2<" + tag + ">3";
    var container = document.createElement("body");
    container.innerHTML = markup;

    if (container.childNodes.length == 1)
        return "allowed";
    if (container.firstChild.lastChild && container.firstChild.lastChild.tagName && container.firstChild.lastChild.tagName.toLowerCase() == tag)
        return "allowed";

    return "closed";
}

function log(message)
{
    document.getElementById("console").appendChild(document.createTextNode(message + "\n\r"));
}

var leafTags = [
    "address",
    "blockquote",
    "center",
    "dir",
    "div",
    "dl",
    "fieldset",
    "h1",
    "h2",
    "h3",
    "h4",
    "h5",
    "h6",
    "menu",
    "ol",
    "p",
    "ul",

    "pre",
    "listing",

    "form",

    "hr",

    "li",

    "dd",
    "dt",

    "plaintext",

    "table",
];

var intermediateTags = [
    "a",            // formatting

    "b",            // formatting
    "big",          // formatting
    "em",           // formatting
    "i",            // formatting
    "s",            // formatting
    "small",        // formatting
    "strike",       // formatting
    "strong",       // formatting
    "tt",           // formatting
    "u",            // formatting

    "abbr",
    "acronym",
    "bdo",
    "cite",
    "code",
    "dfn",
    "kbd",
    "q",
    "samp",
    "sub",
    "sup",
    "var",

    "font",         // formatting

    "nobr",         // formatting

    "button",       // scoping

    "applet",       // scoping
    "object",       // scoping

    "span",         // phrasing
    "del",          // phrasing?
    "ins",          // phrasing?

    "marquee",      // scoping
];

if (window.testRunner)
    testRunner.dumpAsText();

var headerRow = document.getElementById("header-row");
for (var i = 0; i < intermediateTags.length; ++i)
    headerRow.appendChild(document.createElement("th")).appendChild(document.createTextNode(intermediateTags[i]));

for (var i = 0; i < leafTags.length; ++i) {
    var tag = leafTags[i];
    var row = document.getElementById("results").appendChild(document.createElement("tr"));
    row.appendChild(document.createElement("td")).appendChild(document.createTextNode(tag));
    var asChild = testAsChild(tag);
    var cell = row.appendChild(document.createElement("td"));
    cell.className = asChild;
    cell.appendChild(document.createTextNode(asChild == "allowed" ? "+" : asChild == "closed" ? "-" : "?"));
    for (var j = 0; j < intermediateTags.length; ++j) {
        var intermediateTag = intermediateTags[j];
        var asGrandchild = testAsGrandchild(tag, intermediateTag);
        cell = row.appendChild(document.createElement("td"));
        cell.className = asGrandchild;
        cell.appendChild(document.createTextNode(asGrandchild == "allowed" ? "+" : asGrandchild == "closed" ? "-" : "?"));
    }
}
