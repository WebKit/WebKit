if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function createRectangleTest(elementId, stylesheetId, bounds, shapeBounds, units, content) {
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
    var rectangleBounds = [shapeBounds.x + units, shapeBounds.y + units, shapeBounds.width + units, shapeBounds.height + units];
    rules.push('-webkit-shape-inside: rectangle(' + rectangleBounds.join(',') + ')');
    rules.push('position: relative');
    stylesheet.insertRule('#' + elementId + '{' + rules.join(';') + '}');

    rules = [];
    rules.push('left: ' + (shapeBounds.x - 1) + units, 'top: ' + (shapeBounds.y - 1) + units, 'width: ' + rectangleBounds[2], 'height: ' + rectangleBounds[3]);
    rules.push('position: absolute', 'display: block', 'content: \' \'');
    rules.push('border: 1px solid blue');
    stylesheet.insertRule('#' + elementId + ':before{' + rules.join(';') + '}');
    if (content)
        elem.innerHTML = content;
}

function createRectangleTestResult(elementId, stylesheetId, bounds, shapeBounds, units, content) {
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
    stylesheet.insertRule('#' + elementId + '{' + rules.join(';') + '}');

    rules = [];
    rules.push('left: ' + (shapeBounds.x - 1) + units, 'top: ' + (shapeBounds.y - 1) + units, 'width: ' + shapeBounds.width + units, 'height: ' + shapeBounds.height + units);
    rules.push('position: absolute', 'display: block', 'content: \' \'');
    rules.push('border: 1px solid blue');
    stylesheet.insertRule('#' + elementId + ':before{' + rules.join(';') + '}');
    if (content)
        elem.innerHTML = content;
}
