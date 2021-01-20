log = function(msg)
{
    document.getElementById('console').appendChild(document.createTextNode(msg + "\n"));
}

hasAlmostStandardsModeQuirk = function(doc)
{
    var div = doc.createElement('div');
    div.innerHTML = "<img src='' style='background-color: green; width: 100px; height: 100px'><br><img src='' style='background-color: green; width: 100px; height: 100px'>";
    doc.body.appendChild(div);
    var hasQuirk = doc.defaultView.getComputedStyle(div, "").getPropertyValue("height") == "200px";
    doc.body.removeChild(div);
    return hasQuirk;
}

testDoctype = function(doc, expected, doctype)
{
    var mode = "NOT SET";
    if (doc.compatMode == "CSS1Compat") {
        if (hasAlmostStandardsModeQuirk(doc)) {
            mode = "Almost Standards";
        } else {
            mode = "Standards";
        }
    } else if (doc.compatMode == "BackCompat") {
        mode = "Quirks";
    } else {
        mode = "Your browser does not support document.compatMode";
    }

    if (mode == expected) {
        log("PASS: the Doctype was " + mode + " as expected.");
    } else {
        log("FAIL: the Doctype was " + mode + ".  It was expected to be " + expected + ".  Doctype: " + doctype);
    }
}
