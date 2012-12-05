function generatePolygon(width, height, fontSize, points, content, elementId) {
    if (window.internals)
        window.internals.settings.setCSSExclusionsEnabled(true);
    var div = createOrInsert(elementId);
    var polygon = points.map(function(elem, index, array) {
        return elem.toString() + 'px' + (index < array.length - 1 && index % 2 == 1 ? ',' : '');
    }).join(' ');
    polygon = 'polygon(' + polygon + ')';
    div.style.setProperty('-webkit-shape-inside', polygon);
    div.style.setProperty('width', width + 'px');
    div.style.setProperty('height', height + 'px');
    div.style.setProperty('font', fontSize + 'px/1 Ahem, sans-serif');
    div.style.setProperty('color', 'green');
    div.style.setProperty('word-wrap', 'break-word');
    div.innerHTML = content;
}

function simulateWithText(width, height, fontSize, content, elementId) {
    var div = createOrInsert(elementId);
    div.style.setProperty('width', width + 'px');
    div.style.setProperty('height', height + 'px');
    div.style.setProperty('font', fontSize + 'px/1 Ahem, sans-serif');
    div.style.setProperty('color', 'green');
    div.innerHTML = content;
}

function createOrInsert(elementId) {
    if (elementId)
        return document.getElementById(elementId);

    var div = document.createElement('div');
    if (document.body.childNodes.length)
        docuemnt.body.insertBefore(div, document.body.childNodes[0]);
    else
        document.body.appendChild(div);
    return div;
}