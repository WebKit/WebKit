function drawTextRectangle(elementId, stylesheetId, bounds, shapeBounds, units, content) {
    var elem;
    if (elementId)
        elem = document.getElementById(elementId);
    else {
        elem = document.createElement('div');
        elem.setAttribute('id', elementId);
        document.appendChild(elem);
    }

    var stylesheet = document.getElementById(stylesheetId).sheet;
    var rules = [];
    for (var i in bounds)
        rules.push(i + ':' + bounds[i] + units);
    var rectangleBounds = {
        top: shapeBounds.x + units,
        left: shapeBounds.y + units,
        bottom: (shapeBounds.y + shapeBounds.height) + units,
        right: (shapeBounds.x + shapeBounds.width) + units
    };
    rules.push('-webkit-shape-inside: polygon(' +
        rectangleBounds.left + " " + rectangleBounds.top + "," +
        rectangleBounds.right + " " + rectangleBounds.top + "," +
        rectangleBounds.right + " " + rectangleBounds.bottom + "," +
        rectangleBounds.left + " " + rectangleBounds.bottom + ')');
    rules.push('position: relative');
    rules.push('overflow-wrap: break-word');
    stylesheet.insertRule('#' + elementId + '{' + rules.join(';') + '}');

    rules = [];
    rules.push('left: ' + (shapeBounds.x - 1) + units, 'top: ' + (shapeBounds.y - 1) + units, 'width: ' + shapeBounds.width + units, 'height: ' + shapeBounds.height + units);
    rules.push('position: absolute', 'display: block', 'content: \' \'');
    rules.push('border: 1px solid blue');
    stylesheet.insertRule('#' + elementId + ':before{' + rules.join(';') + '}');
    if (content)
        elem.innerHTML = content;
}

function drawExpectedRectangle(elementId, stylesheetId, bounds, shapeBounds, units, content) {
    var elem;
    if (elementId)
        elem = document.getElementById(elementId);
    else {
        elem = document.createElement('div');
        elem.setAttribute('id', elementId);
        document.appendChild(elem);
    }

    var stylesheet = document.getElementById(stylesheetId).sheet;
    var rules = [];
    rules.push('width: ' + shapeBounds.width + units, 'height: ' + shapeBounds.height + units);
    rules.push('padding-left: ' + shapeBounds.x + units, 'padding-right: ' + (bounds.width - shapeBounds.width - shapeBounds.x) + units);
    rules.push('padding-top: ' + shapeBounds.y + units, 'padding-bottom: ' + (bounds.height - shapeBounds.height - shapeBounds.y) + units);
    rules.push('position: relative');
    rules.push('overflow-wrap: break-word');
    stylesheet.insertRule('#' + elementId + '{' + rules.join(';') + '}');

    rules = [];
    rules.push('left: ' + (shapeBounds.x - 1) + units, 'top: ' + (shapeBounds.y - 1) + units, 'width: ' + shapeBounds.width + units, 'height: ' + shapeBounds.height + units);
    rules.push('position: absolute', 'display: block', 'content: \' \'');
    rules.push('border: 1px solid blue');
    stylesheet.insertRule('#' + elementId + ':before{' + rules.join(';') + '}');
    if (content)
        elem.innerHTML = content;
}
