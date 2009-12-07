description("Patterns shouldn't crash for size < 0.5 .");

var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
document.documentElement.insertBefore(svg, document.documentElement.firstChild);

var pattern = document.createElementNS("http://www.w3.org/2000/svg", "pattern");
pattern.setAttribute("id", "pattern");
pattern.setAttribute("width", "0.1");
pattern.setAttribute("height", "0.1");
pattern.setAttribute("overflow", "visible");
pattern.setAttribute("patternUnits", "userSpaceOnUse");

var patternRect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
patternRect.setAttribute("width", "1");
patternRect.setAttribute("height", "1");
patternRect.setAttribute("fill", "green");
pattern.appendChild(patternRect);

svg.appendChild(pattern);

var rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
rect.setAttribute("width", "1");
rect.setAttribute("height", "1");
rect.setAttribute("fill", "url(#pattern)");

svg.appendChild(rect);

var successfullyParsed = true;
