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

shouldBeEqualToString('testComputedStyle("-webkit-from-flow(\'first-flow\')")', "first-flow");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(\'first flow\')")', "'first flow'");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(\'auto\')")', "auto");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(auto)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(initial)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(inherit)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow()")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(1)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(1.2)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(-1)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(12px)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(\'\')")', "''");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(first flow)")', "");
shouldBeEqualToString('testComputedStyle("-webkit-from-flow(first-flow)")', "");

successfullyParsed = true;
