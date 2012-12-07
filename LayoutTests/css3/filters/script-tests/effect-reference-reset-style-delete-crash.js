description("Test that no crash occurs when a SVG filter is deleted that used to be referenced by an HTML element but is no longer.");

html = document.getElementById('html');
html.style = "";
svg = document.getElementById('svg');
svg.parentNode.removeChild(svg);

successfullyParsed = true;
