function removeAllInDirectorySync(directory) {
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

if (this.importScripts && !this.webkitRequestFileSystem) {
    debug('This test requires FileSystem API.');
    finishJSTest();
}
