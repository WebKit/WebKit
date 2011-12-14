var testCases = [
    {
        name: 'CopyFileSimple',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'}
        ],
        tests: [
            function(helper) { helper.copy('/a/b', '/a', 'c'); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/a/c'}
        ],
    },
    {
        name: 'CopyDirectorySimple',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.copy('/a/b', '/a', 'c'); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/c', isDirectory:true}
        ],
    },
    {
        name: 'CopyFileToDifferentDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.copy('/a/b', '/c', 'd'); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c/d'}
        ],
    },
    {
        name: 'CopyFileWithEmptyName',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.copy('/a/b', '/c', null); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c/b'}
        ],
    },
    {
        name: 'CopyFileWithEmptyNameToSameDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
        tests: [
            function(helper) { helper.copy('/a/b', '/a', null, FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
    },
    {
        name: 'CopyFileWithSameName',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
        tests: [
            function(helper) { helper.copy('/a/b', '/a', 'b', FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
    },
    {
        name: 'CopyForNonExistentEntry',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.remove('/a/b'); },
            function(helper) { helper.copy('/a/b', '/c', 'b', FileError.NOT_FOUND_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/c', isDirectory:true},
        ],
    },
    {
        name: 'CopyEntryToNonExistentDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.remove('/c'); },
            function(helper) { helper.copy('/a/b', '/c', 'b', FileError.NOT_FOUND_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
    },
    {
        name: 'CopyEntryToItsChild',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.copy('/a', '/a/b', 'd', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/a/b', '/a/b/c', 'd', FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c', isDirectory:true},
        ],
    },
    {
        name: 'CopyRecursive',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c'},
            {fullPath:'/a/b/d'},
            {fullPath:'/b', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.copy('/a', '/b', 'a'); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c'},
            {fullPath:'/a/b/d'},
            {fullPath:'/b/a', isDirectory:true},
            {fullPath:'/b/a/b', isDirectory:true},
            {fullPath:'/b/a/b/c'},
            {fullPath:'/b/a/b/d'},
        ],
    },
    {
        name: "OverwritingCopyFileToFile",
        precondition: [
            {fullPath:"/a"},
            {fullPath:"/b"},
        ],
        tests: [
            function(helper) {helper.copy("/a","/","b");}
        ],
        postcondition: [
            {fullPath:"/a"},
            {fullPath:"/b"},
        ],
    },
    {
        name: "OverwritingCopyDirectoryToEmptyDirectory",
        precondition: [
            {fullPath:"/a", isDirectory:true},
            {fullPath:"/a/b"},
            {fullPath:"/c", isDirectory:true},
        ],
        tests: [
            function(helper) {helper.copy("/a","/","c");}
        ],
        postcondition: [
            {fullPath:"/a", isDirectory:true},
            {fullPath:"/a/b"},
            {fullPath:"/c", isDirectory:true},
            {fullPath:"/c/b"},
        ],
    },
    {
        name: "OverwritingCopyFileToDirectory",
        precondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
        ],
        tests: [
            function(helper) {helper.copy("/a","/","b",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
        ],
    },
    {
        name: "OverwritingCopyDirectoryToFile",
        precondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/b"},
        ],
        tests: [
            function(helper) {helper.copy("/a","/","b",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/b"},
        ],
    },
    {
        name: "OverwritingCopyFileToNonemptyDirectory",
        precondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
            {fullPath:"/b/c"},
        ],
        tests: [
            function(helper) {helper.copy("/a","/","b",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
            {fullPath:"/b/c"},
        ],
    },
    {
        name: "OverwritingCopyDirectoryToNonemptyDirectory",
        precondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/a/b"},
            {fullPath:"/c", isDirectory: true},
            {fullPath:"/c/d"},
        ],
        tests: [
            function(helper) {helper.copy("/a","/","c",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/a/b"},
            {fullPath:"/c", isDirectory: true},
            {fullPath:"/c/d"},
        ],
    },
];
