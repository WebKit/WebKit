
function addToLog(s)
{
    let logging = document.getElementById('logging');
    logging.textContent += s + "\n";
    logging.scrollTop = 400000;
}

function clearLog()
{
    let logging = document.getElementById('logging');
    logging.textContent = '';
}

function stringFromRect(rect)
{
    return `${rect.x}, ${rect.y} ${rect.width} x ${rect.height}`;
}

function intersectedCallback(entries)
{
    addToLog('');
    for (var entry of entries) {
        addToLog('time: ' + entry.time.toFixed(2));
        addToLog('intersection:        ' + stringFromRect(entry.intersectionRect));
        addToLog('intersection ratio:  ' + entry.intersectionRatio.toFixed(3));
    }
}
