description("Test that no crash occurs when a SVG filter is deleted that was referenced by a deleted HTML element.");

html = document.getElementById('html');
html.parentNode.removeChild(html);
svg = document.getElementById('svg');
svg.parentNode.removeChild(svg);

successfullyParsed = true;
