if (window.testRunner)
    testRunner.dumpAsText();

description("Test strict color parsing on SVG presentation attributes.")
createSVGTestCase();

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rootSVGElement.appendChild(rect);


// Testing 'fill'
// The default for fill is #000000.
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#000000");

// Set the fill color to green.
rect.setAttribute("fill", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#008000");

// Set following colors should be invalid.
rect.setAttribute("fill", "f00");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).fill");
// Reset to green.
rect.setAttribute("fill", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#008000");

rect.setAttribute("fill", "ff00");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).fill");
// Reset to green.
rect.setAttribute("fill", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#008000");

rect.setAttribute("fill", "ff0000");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).fill");
// Reset to green.
rect.setAttribute("fill", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#008000");

rect.setAttribute("fill", "ff00");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).fill");
// Reset to green.
rect.setAttribute("fill", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#008000");

rect.setAttribute("fill", "");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#000000");
// Reset to green.
rect.setAttribute("fill", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).fill", "#008000");



// Testing 'stroke'
// The default stroke value should be 'none'.
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "none");

// Set the stroke color to green.
rect.setAttribute("stroke", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "#008000");

// Set following colors should be invalid.
rect.setAttribute("stroke", "f00");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).stroke");
// Reset to green.
rect.setAttribute("stroke", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "#008000");

rect.setAttribute("stroke", "ff00");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).stroke");
// Reset to green.
rect.setAttribute("stroke", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "#008000");

rect.setAttribute("stroke", "ff0000");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).stroke");
// Reset to green.
rect.setAttribute("stroke", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "#008000");

rect.setAttribute("stroke", "ff00");
shouldBeNull("document.defaultView.getComputedStyle(rect, null).stroke");
// Reset to green.
rect.setAttribute("stroke", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "#008000");

rect.setAttribute("stroke", "");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "none");
// Reset to green.
rect.setAttribute("stroke", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stroke", "#008000");


// Testing 'color'
// The default for color is rgb(0, 0, 0).
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 0, 0)");

// Set color to green.
rect.setAttribute("color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 128, 0)");

// Set following colors should be invalid.
rect.setAttribute("color", "f00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 128, 0)");

rect.setAttribute("color", "ff00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 128, 0)");

rect.setAttribute("color", "ff0000");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 128, 0)");

rect.setAttribute("color", "ff00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 128, 0)");

rect.setAttribute("color", "");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).color", "rgb(0, 128, 0)");


// Testing 'stop-color'
// The default for stop-color is rgb(0, 0, 0).
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 0, 0)");

// Set color to green.
rect.setAttribute("stop-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 128, 0)");

// Set following colors should be invalid.
rect.setAttribute("stop-color", "f00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("stop-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 128, 0)");

rect.setAttribute("stop-color", "ff00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("stop-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 128, 0)");

rect.setAttribute("stop-color", "ff0000");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("stop-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 128, 0)");

rect.setAttribute("stop-color", "ff00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("stop-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 128, 0)");

rect.setAttribute("stop-color", "");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("stop-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).stopColor", "rgb(0, 128, 0)");


// Testing 'flood-color'
// The default for flood-color is rgb(0, 0, 0).
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 0, 0)");

// Set color to green.
rect.setAttribute("flood-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 128, 0)");

// Set following colors should be invalid.
rect.setAttribute("flood-color", "f00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("flood-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 128, 0)");

rect.setAttribute("flood-color", "ff00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("flood-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 128, 0)");

rect.setAttribute("flood-color", "ff0000");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("flood-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 128, 0)");

rect.setAttribute("flood-color", "ff00");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("flood-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 128, 0)");

rect.setAttribute("flood-color", "");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 0, 0)");
// Reset to green.
rect.setAttribute("flood-color", "green");
shouldBeEqualToString("document.defaultView.getComputedStyle(rect, null).floodColor", "rgb(0, 128, 0)");

var successfullyParsed = true;

completeTest();
