var testCases = [
    {
        name: 'MoveFileSimple',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'}
        ],
        tests: [
            function(helper) { helper.move('/a/b', '/a', 'c'); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', nonexistent:true},
            {fullPath:'/a/c'}
        ],
    },
    {
        name: 'MoveDirectorySimple',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.move('/a/b', '/a', 'c'); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', nonexistent:true},
            {fullPath:'/a/c', isDirectory:true}
        ],
    },
    {
        name: 'MoveFileToDifferentDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.move('/a/b', '/c', 'd'); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', nonexistent:true},
            {fullPath:'/c/d'}
        ],
    },
    {
        name: 'MoveFileWithEmptyName',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.move('/a/b', '/c', null); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', nonexistent:true},
            {fullPath:'/c/b'}
        ],
    },
    {
        name: 'MoveFileWithEmptyNameToSameDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
        tests: [
            function(helper) { helper.move('/a/b', '/a', null, FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
    },
    {
        name: 'MoveFileWithSameName',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
        tests: [
            function(helper) { helper.move('/a/b', '/a', 'b', FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
    },
    {
        name: 'MoveForNonExistentEntry',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.remove('/a/b'); },
            function(helper) { helper.move('/a/b', '/c', 'b', FileError.NOT_FOUND_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/c', isDirectory:true},
        ],
    },
    {
        name: 'MoveEntryToNonExistentDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.remove('/c'); },
            function(helper) { helper.move('/a/b', '/c', 'b', FileError.NOT_FOUND_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b'},
        ],
    },
    {
        name: 'MoveEntryToItsChild',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.move('/a', '/a/b', 'd', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.move('/a/b', '/a/b/c', 'd', FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c', isDirectory:true},
        ],
    },
    {
        name: 'MoveRecursive',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c'},
            {fullPath:'/a/b/d'},
            {fullPath:'/b', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.move('/a', '/b', 'a'); },
        ],
        postcondition: [
            {fullPath:'/a', nonexistent:true},
            {fullPath:'/b/a', isDirectory:true},
            {fullPath:'/b/a/b', isDirectory:true},
            {fullPath:'/b/a/b/c'},
            {fullPath:'/b/a/b/d'},
        ],
    },
    {
        name: "OverwritingMoveFileToFile",
        precondition: [
            {fullPath:"/a"},
            {fullPath:"/b"},
        ],
        tests: [
            function(helper) {helper.move("/a","/","b");}
        ],
        postcondition: [
            {fullPath:"/b"},
        ],
    },
    {
        name: "OverwritingMoveDirectoryToEmptyDirectory",
        precondition: [
            {fullPath:"/a", isDirectory:true},
            {fullPath:"/a/b"},
            {fullPath:"/c", isDirectory:true},
        ],
        tests: [
            function(helper) {helper.move("/a","/","c");}
        ],
        postcondition: [
            {fullPath:"/c", isDirectory:true},
            {fullPath:"/c/b"},
            {fullPath:"/a", nonexistent:true},
        ],
    },
    {
        name: "OverwritingMoveFileToDirectory",
        precondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
        ],
        tests: [
            function(helper) {helper.move("/a","/","b",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
        ],
    },
    {
        name: "OverwritingMoveDirectoryToFile",
        precondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/b"},
        ],
        tests: [
            function(helper) {helper.move("/a","/","b",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/b"},
        ],
    },
    {
        name: "OverwritingMoveFileToNonemptyDirectory",
        precondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
            {fullPath:"/b/c"},
        ],
        tests: [
            function(helper) {helper.move("/a","/","b",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a"},
            {fullPath:"/b", isDirectory: true},
            {fullPath:"/b/c"},
        ],
    },
    {
        name: "OverwritingMoveDirectoryToNonemptyDirectory",
        precondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/a/b"},
            {fullPath:"/c", isDirectory: true},
            {fullPath:"/c/d"},
        ],
        tests: [
            function(helper) {helper.move("/a","/","c",FileError.INVALID_MODIFICATION_ERR);}
        ],
        postcondition: [
            {fullPath:"/a", isDirectory: true},
            {fullPath:"/a/b"},
            {fullPath:"/c", isDirectory: true},
            {fullPath:"/c/d"},
        ],
    },
];
