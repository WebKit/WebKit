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
            else if (evt.data == 'requestFileSystemSync') {
                if (!this.webkitRequestFileSystemSync)
                    return;
                webkitRequestFileSystemSync(this.TEMPORARY, 100);
            } else if (evt.data == 'fileSyncOperations') {
                if (!this.webkitRequestFileSystemSync)
                    return;
                // Do many different sync filesystem operations. If this starts crashing,
                // then a simple investigation would be to isolate these commands.
                var fileSystem = webkitRequestFileSystemSync(this.TEMPORARY, 100);

                removeAllInDirectorySync(fileSystem.root);

                // Stage 1 (prepare)
                var a = fileSystem.root.getFile('a', {create:true});
                var b = fileSystem.root.getDirectory('b', {create:true});
                var c = fileSystem.root.getDirectory('c', {create:true});
                var d = fileSystem.root.getFile('d', {create:true});

                // Stage 2 (test)
                var a_copy = a.copyTo(b, 'tmp');
                var metadata = a.getMetadata();
                var b_parent = b.getParent();
                var c_copy = c.copyTo(fileSystem.root, 'f');
                var d_new = d.moveTo(fileSystem.root, 'd2');
                var e = fileSystem.root.getFile('e', {create:true});

                // Verify
                var reader = fileSystem.root.createReader();
                var dirsCount = 0;
                var paths = [];
                do {
                    var entries = reader.readEntries();
                    for (var i = 0; i < entries.length; ++i) {
                    paths.push(entries[i].fullPath);
                    if (entries[i].isDirectory)
                        dirsCount++;
                    }
                } while (entries.length);

                removeAllInDirectorySync(fileSystem.root);
            }
        }
    } catch (e) {
        // Purposely ignore any exceptions. Since the whole purpose of this test is to try
        // to interrupt the synchronous operations, they will naturally throw exceptions,
        // but which ones throw exception isn't determinant and we don't want random output
        // showing up as a console message.
    }
}

postMessage('started');
