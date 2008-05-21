function endTest() {
    console.profileEnd();
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
