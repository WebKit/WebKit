function removeAllInDirectorySync(directory)
{
    if (!directory)
        return;
    var reader = directory.createReader();
    do {
        var entries = reader.readEntries();
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].isDirectory)
                entries[i].removeRecursively();
            else
                entries[i].remove();
        }
    } while (entries.length);
}

onmessage = function(evt)
{
    try {
        // Increase the change of getting caught doing a sync operation
        // by repeating the opration multiple times.
        for (var i = 0; i < 10; ++i) {
            if (evt.data == 'openDatabaseSync')
                openDatabaseSync('', '', '', 1);
        }
    } catch (e) {
        // Purposely ignore any exceptions. Since the whole purpose of this test is to try
        // to interrupt the synchronous operations, they will naturally throw exceptions,
        // but which ones throw exception isn't determinant and we don't want random output
        // showing up as a console message.
    }
}

postMessage('started');
