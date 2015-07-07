description('Test the disabled property on an svg style element.');

var console = document.getElementById('console');

// Setup
function createStyle(ns, type, ruletext) {
    var style = document.createElementNS(ns, "style");
    var rules = document.createTextNode(ruletext);
    style.appendChild(rules);
    style.type = type;
    return style;
}

var xmlns = "http://www.w3.org/2000/svg";
var svg = document.createElementNS(xmlns, "svg");
svg.style.display = "block";

var defs = document.createElementNS(xmlns, "defs");
var styleElement = createStyle(xmlns, "text/css", "rect { fill: #0000ff; }");
var otherStyleElement = createStyle(xmlns, "foo/bar", "");
defs.appendChild(styleElement);
defs.appendChild(otherStyleElement);
svg.appendChild(defs);

var rect = document.createElementNS(xmlns, "rect");
rect.setAttribute("width", 100);
rect.setAttribute("height", 100);
svg.appendChild(rect);

document.body.appendChild(svg);

// Simple case
shouldBeFalse('styleElement.disabled');
shouldBe('window.getComputedStyle(rect).fill', '"#0000ff"');

styleElement.disabled = true
shouldBeTrue('styleElement.disabled');
shouldBe('window.getComputedStyle(rect).fill', '"#ff0000"');

// Test disconnected element
var newStyleElement = document.createElementNS(xmlns, 'style');
shouldBeFalse('newStyleElement.disabled');
newStyleElement.disabled = true
shouldBeFalse('newStyleElement.disabled');

// Test non-CSS element
shouldBeFalse('otherStyleElement.disabled');
otherStyleElement.disabled = true
shouldBeFalse('otherStyleElement.disabled');

document.body.removeChild(svg);