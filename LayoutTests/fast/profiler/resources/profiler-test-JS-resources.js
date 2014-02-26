function endTest() {
    console.profileEnd();
    printProfilesDataWithoutTime();

    if (window.testRunner)
        testRunner.notifyDone();
}

function insertGivenText(text) {
    var paragraph = document.createElement("p");
    paragraph.appendChild(document.createTextNode(text));
    paragraph.style.display = "none"; // Hidden since this isn't important in the test results.

    document.getElementById("output").appendChild(paragraph);
}

function insertNewText() {
    var paragraph = document.createElement("p");
    paragraph.appendChild(document.createTextNode("This is inserted Text"));
    paragraph.style.display = "none"; // Hidden since this isn't important in the test results.

    document.getElementById("output").appendChild(paragraph);
}

function arrayOperatorFunction(arrayElement) {
    return arrayElement + 5;
}

var anonymousFunction = function () { insertNewText(); };
var anotherAnonymousFunction = function () { insertGivenText("Another anonymous function was called.") };

function intermediaryFunction()
{
    anonymousFunction();
}

function isEqualToFive(input)
{
    return input === 5;
}

function startProfile(title)
{
    console.profile(title);
}

function printHeavyProfilesDataWithoutTime()
{
    var preElement = document.createElement("pre");
    preElement.appendChild(document.createTextNode("\n"));

    var profiles = internals.consoleProfiles;
    for (var i = 0; i < profiles.length; ++i) {
        preElement.appendChild(document.createTextNode("Profile title: " + profiles[i].title + "\n"));
        printProfileNodeWithoutTime(preElement, profiles[i].heavyProfile.head, 0);
        preElement.appendChild(document.createTextNode("\n"));
    }

    document.getElementById("output").appendChild(preElement);
}

function printProfilesDataWithoutTime()
{
    var preElement = document.createElement("pre");
    preElement.appendChild(document.createTextNode("\n"));

    var profiles = internals.consoleProfiles;
    for (var i = 0; i < profiles.length; ++i) {
        preElement.appendChild(document.createTextNode("Profile title: " + profiles[i].title + "\n"));
        printProfileNodeWithoutTime(preElement, profiles[i].head, 0);
        preElement.appendChild(document.createTextNode("\n"));
    }

    document.getElementById("output").appendChild(preElement);
}

function printProfileNodeWithoutTime(preElement, node, indentLevel)
{
    var space = "";
    for (var i = 0; i < indentLevel; ++i)
        space += "   "

    ++indentLevel;

    var strippedURL = node.url.replace(/.*\//, "");
    if (!strippedURL)
        strippedURL = "(no file)";

    var line = space + node.functionName + " " + strippedURL + " (line " + node.lineNumber + ":" + node.columnNumber + ")\n";
    preElement.appendChild(document.createTextNode(line));

    var children = node.children();
    for (var i = 0; i < children.length; ++i)
        printProfileNodeWithoutTime(preElement, children[i], indentLevel);
}
