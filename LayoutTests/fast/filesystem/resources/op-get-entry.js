var testCases = [
    {
        name: 'CreateSimple',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true}); },
            function(helper) { helper.getFile('/', 'b', {create:true}); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'},
        ],
    },
    {
        name: 'CreateNested',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true}); },
            function(helper) { helper.getDirectory('/a', 'b', {create:true}); },
            function(helper) { helper.getDirectory('/a/b', 'c', {create:true}); },
            function(helper) { helper.getDirectory('/a/b/c', 'd', {create:true}); },
            function(helper) { helper.getFile('/a/b/c/d', 'e', {create:true}); },
        ],
        postcondition: [
            {fullPath:'/a/b/c/d/e'},
        ],
    },
    {
        name: 'CreateNestedWithAbsolutePath',
        precondition: [
            {fullPath:'/dummy', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.getDirectory('/dummy', '/a', {create:true}); },
            function(helper) { helper.getDirectory('/dummy', '/a/b', {create:true}); },
            function(helper) { helper.getDirectory('/dummy', '/a/b/c', {create:true}); },
            function(helper) { helper.getDirectory('/dummy', '/a/b/c/d', {create:true}); },
            function(helper) { helper.getFile('/dummy', '/a/b/c/d/e', {create:true}); }
        ],
        postcondition: [
            {fullPath:'/dummy', isDirectory:true},
            {fullPath:'/a/b/c/d/e'},
        ],
    },
    {
        name: 'CreateNestedWithRelativePath',
        precondition: [
            {fullPath:'/a', isDirectory:true},
        ],
        tests: [
            // FIXME: For now they throw an error because they fail the check for restricted-names: 'a path component should not end with period'.
            function(helper) { helper.getDirectory('/a', './b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getDirectory('/a', '../b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getDirectory('/a', '/a/../../b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [ ],
    },
    {
        name: 'GetExistingEntry',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true}); },
            function(helper) { helper.getFile('/', 'b', {create:true}); },
            function(helper) { helper.getDirectory('/', 'a'); },
            function(helper) { helper.getFile('/', 'b'); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'},
        ],
    },
    {
        name: 'GetNonExistent',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {}, FileError.NOT_FOUND_ERR); },
            function(helper) { helper.getFile('/', 'b', {}, FileError.NOT_FOUND_ERR); },
            function(helper) { helper.getDirectory('/', '/nonexistent/a', {create:true}, FileError.NOT_FOUND_ERR); },
            function(helper) { helper.getFile('/', '/nonexistent/b', {create:true}, FileError.NOT_FOUND_ERR); }
        ],
        postcondition: [ ],
    },
    {
        name: 'GetFileForDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.getFile('/', 'a', {}, FileError.INVALID_STATE_ERR); },
            function(helper) { helper.getFile('/', '/a', {}, FileError.INVALID_STATE_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true}
        ],
    },
    {
        name: 'GetDirectoryForFile',
        precondition: [
            {fullPath:'/a'}
        ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {}, FileError.INVALID_STATE_ERR); },
            function(helper) { helper.getDirectory('/', '/a', {}, FileError.INVALID_STATE_ERR); },
        ],
        postcondition: [
            {fullPath:'/a'}
        ],
    },
    {
        name: 'CreateWithExclusive',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'}
        ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true, exclusive:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'b', {create:true, exclusive:true}, FileError.INVALID_MODIFICATION_ERR); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'}
        ],
    },
];
