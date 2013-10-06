description('Tests parsing of -webkit-flow-from property');
 
function testCSSText(declaration) {
 var div = document.createElement("div");
 div.setAttribute("style", declaration);
 return div.style.webkitFlowFrom;
}

function testComputedStyle(declaration) {
 var div = document.createElement("div");
 document.body.appendChild(div);
 div.style.setProperty("-webkit-flow-from", declaration);

 var contentComputedValue = getComputedStyle(div).getPropertyValue("-webkit-flow-from");
 document.body.removeChild(div);
 return contentComputedValue;
}

shouldBeEqualToString('testCSSText("-webkit-flow-from: first-flow")', "first-flow");
shouldBeEqualToString('testCSSText("-webkit-flow-from: none")', "none");
shouldBeEqualToString('testCSSText("-webkit-flow-from: ")', "");
shouldBeEqualToString('testCSSText("-webkit-flow-from: \'first-flow\'")', "");
shouldBeEqualToString('testCSSText("-webkit-flow-from: 1")', "");
shouldBeEqualToString('testCSSText("-webkit-flow-from: 1.2")', "");
shouldBeEqualToString('testCSSText("-webkit-flow-from: -1")', "");
shouldBeEqualToString('testCSSText("-webkit-flow-from: 12px")', "");
shouldBeEqualToString('testCSSText("-webkit-from-flow: first flow")', "");

shouldBeEqualToString('testComputedStyle("first-flow")', "first-flow");
shouldBeEqualToString('testComputedStyle("\'first flow\'")', "none");
shouldBeEqualToString('testComputedStyle("none")', "none");
shouldBeEqualToString('testComputedStyle("initial")', "none");
shouldBeEqualToString('testComputedStyle("inherit")', "none");
shouldBeEqualToString('testComputedStyle("1")', "none");
shouldBeEqualToString('testComputedStyle("1.2")', "none");
shouldBeEqualToString('testComputedStyle("-1")', "none");
shouldBeEqualToString('testComputedStyle("12px")', "none");
