function endTest() {
    console.profileEnd();
    printProfilesDataWithoutTime();
    
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

function insertGivenText(text) {
    var newP = document.createElement("p");
    var textNode =document.createTextNode(text);
    newP.appendChild(textNode);
    var output = document.getElementById("output");
    output.appendChild(newP);
}

function insertNewText() {
    var newP = document.createElement("p");
    var textNode =document.createTextNode("This is inserted Text");
    newP.appendChild(textNode);
    var output = document.getElementById("output");
    output.appendChild(newP);
}

function arrayOperatorFunction(arrayElement) {
    return arrayElement + 5;
}

var anonymousFunction = function () { insertNewText(); };

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
    var profiles = console.profiles;
    for (var i = 0; i < profiles.length; ++i) {
        console.log(profiles[i].title);
        printProfileNodeWithoutTime(profiles[i].heavyProfile.head, 0);
    }
}

function printProfilesDataWithoutTime()
{
    var profiles = console.profiles;
    for (var i = 0; i < profiles.length; ++i) {
        console.log(profiles[i].title);
        printProfileNodeWithoutTime(profiles[i].treeProfile.head, 0);
    }
}

function printProfileNodeWithoutTime(node, indentLevel)
{
    if (!node.visible)
        return;

    var space = "";
    for (var i = 0; i < indentLevel; ++i)
        space += " "

    ++indentLevel;

    console.log(space + node.functionName + " " + node.url + " " + node.lineNumber);

    var children = node.children;
    for (var i = 0; i < children.length; ++i)
        printProfileNodeWithoutTime(children[i], indentLevel);
}

