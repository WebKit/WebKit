postMessage('started');

onmessage = function(evt)
{
    if (evt.data == 'openDatabaseSync')
        openDatabaseSync('', '', '', 1);
}
