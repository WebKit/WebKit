if (window.testRunner)
    testRunner.dumpAsText();

description("Test CSS <length> parsing on SVG presentation attributes.")
createSVGTestCase();

var text = createSVGElement("text");
text.setAttribute("id", "text");
text.setAttribute("x", "20px");
text.setAttribute("y", "100px");
text.innerHTML = "Test";
rootSVGElement.appendChild(text);

// Test initial value of font-length.
shouldBeEqualToString("document.defaultView.getComputedStyle(text, null).fontSize", "16px");

// Set valid font-size of 100px
text.setAttribute("font-size", "100px");
shouldBeEqualToString("document.defaultView.getComputedStyle(text, null).fontSize", "100px");

// Space between unit and value - invalid according strict parsing rules.
text.setAttribute("font-size", "100  px");
shouldBeEqualToString("document.defaultView.getComputedStyle(text, null).fontSize", "16px");

// Space within the value - invalid according strict parsing rules.
text.setAttribute("font-size", "10 0px");
shouldBeEqualToString("document.defaultView.getComputedStyle(text, null).fontSize", "16px");

var successfullyParsed = true;

completeTest();
