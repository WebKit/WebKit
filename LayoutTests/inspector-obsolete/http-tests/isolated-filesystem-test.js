var initialize_IsolatedFileSystemTest = function() {

InspectorTest.createIsolatedFileSystemManager = function(workspace, fileSystemMapping)
{
    var manager = new MockIsolatedFileSystemManager();
    manager.fileSystemWorkspaceProvider = new WebInspector.FileSystemWorkspaceProvider(manager, workspace);
    manager.fileSystemMapping = fileSystemMapping;
    return manager;
}

var MockIsolatedFileSystem = function(path, files)
{
    this._path = path;
    this._files = files;
};
MockIsolatedFileSystem.prototype = {
    path: function()
    {
        return this._path;
    },

    requestFileContent: function(path, callback)
    {
        callback(this._files[path]);
    },

    setFileContent: function(path, newContent, callback)
    {
        this._files[path] = newContent;
        callback();
    },

    requestFilesRecursive: function(path, callback)
    {
        var files = Object.keys(this._files);
        for (var i = 0; i < files.length; ++i)
            callback(files[i]);
    },
}

var MockIsolatedFileSystemManager = function() {};
MockIsolatedFileSystemManager.prototype = {
    addMockFileSystem: function(path, files)
    {
        var fileSystem = new MockIsolatedFileSystem(path, files);
        this._fileSystems = this._fileSystems || {};
        this._fileSystems[path] = fileSystem;
        this.fileSystemMapping.addFileSystemMapping(path);
        this.dispatchEventToListeners(WebInspector.IsolatedFileSystemManager.Events.FileSystemAdded, fileSystem);
    },

    removeMockFileSystem: function(path)
    {
        var fileSystem = this._fileSystems[path];
        delete this._fileSystems[path];
        this.fileSystemMapping.removeFileSystemMapping(path);
        this.dispatchEventToListeners(WebInspector.IsolatedFileSystemManager.Events.FileSystemRemoved, fileSystem);
    },

    __proto__: WebInspector.Object.prototype
}

};
