description('Tests being able to set content to -webkit-from-flow');

function testCSSText(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.content;
}

function testComputedStyle(declaration) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("content", declaration);

    var contentComputedValue = getComputedStyle(div).getPropertyValue("content");
    document.body.removeChild(div);
    return contentComputedValue;
}

shouldBeEqualToString('testCSSText("content: -webkit-from-flow(\'first-flow\')")', "-webkit-from-flow(first-flow)");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(\'first flow\')")', "-webkit-from-flow('first flow')");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(\'auto\')")', "-webkit-from-flow(auto)");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(auto)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(initial)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(inherit)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow()")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(\'\')")', "-webkit-from-flow('')");
shouldBeEqualToString('testCSSText("content: ;")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(1)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(1.2)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(-1)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(12px)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(first flow)")', "");
shouldBeEqualToString('testCSSText("content: -webkit-from-flow(first-flow)")', "");

shouldBeEqualToString('testComputedStyle("\'first-flow\'")', "first-flow");
shouldBeEqualToString('testComputedStyle("\'first flow\'")', "'first flow'");
shouldBeEqualToString('testComputedStyle("\'auto\'")', "auto");
shouldBeEqualToString('testComputedStyle("auto")', "");
shouldBeEqualToString('testComputedStyle("initial")', "");
shouldBeEqualToString('testComputedStyle("inherit")', "");
shouldBeEqualToString('testComputedStyle("")', "");
shouldBeEqualToString('testComputedStyle("1")', "");
shouldBeEqualToString('testComputedStyle("1.2")', "");
shouldBeEqualToString('testComputedStyle("-1")', "");
shouldBeEqualToString('testComputedStyle("12px")', "");
shouldBeEqualToString('testComputedStyle("\'\'")', "''");
shouldBeEqualToString('testComputedStyle("first flow")', "");
shouldBeEqualToString('testComputedStyle("first-flow")', "");

successfullyParsed = true;
