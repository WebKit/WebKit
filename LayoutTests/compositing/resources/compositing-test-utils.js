function makeDots(width, height, spacing = 10)
{
    for (var row = 0; row < height; ++row) {
        for (var col = 0; col < width; ++col) {
            var dot = document.createElement('div');
            dot.className = 'dot';
            dot.style.left = spacing * col + 'px';
            dot.style.top = spacing * row + 'px';
            document.body.appendChild(dot);
        }
    }
}

function layerTreeWithoutTransforms(layerTreeFlags = 0)
{
    var layerTreeText = internals.layerTreeAsText(document, layerTreeFlags);
    var filteredLines = layerTreeText.split("\n").filter(line => line.indexOf('transform') == -1);
    return filteredLines.join('\n');
}

function dumpLayersWithoutTransforms()
{
    if (window.testRunner) {
        document.getElementById('layers').innerText = layerTreeWithoutTransforms();
        testRunner.notifyDone();
    }
}
