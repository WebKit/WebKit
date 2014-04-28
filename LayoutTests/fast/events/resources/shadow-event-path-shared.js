if (window.testRunner)
    testRunner.dumpAsText();

function log(message) {
    var log = document.getElementById('log');
    if (!log) {
        log = document.createElement('pre');
        log.id = 'log';
        document.body.appendChild(log);
    }
    log.textContent += message + '\n';
}

function targetIdentifier(target) {
    if (target === undefined || target === null)
        return target;
    if (target === window)
        return 'window';
    if (target === document)
        return 'document';
    return target.localName + (target.id ? '#' + target.id : '');
}

function attachListeners(eventname) {
    var targets = Array.prototype.slice.call(document.querySelectorAll('*'));
    targets.push(window);
    targets.push(document);
    targets.forEach(function (target) {
        target.addEventListener(eventname, function (event) {
            log(eventname + '@' + targetIdentifier(target) + '\n'
                + '    target:' + targetIdentifier(event.target) + '\n'
                + '    relatedTarget:' + targetIdentifier(event.relatedTarget) + '\n');
        });
    });
}


attachListeners('mousemove');
attachListeners('mousedown');
attachListeners('mouseover');
attachListeners('mouseout');
attachListeners('mouseenter');
attachListeners('mouseleave');
attachListeners('mouseup');
attachListeners('click');
