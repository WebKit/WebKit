var testCases = [
    {
        name: 'RemoveSimple',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'}
        ],
        tests: [
            function(helper) { helper.remove('/a'); },
            function(helper) { helper.remove('/b'); }
        ],
        postcondition: [
            {fullPath:'/a', nonexistent:true},
            {fullPath:'/b', nonexistent:true}
        ],
    },
    {
        name: 'RemoveNonRecursiveWithChildren',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/c',}
        ],
        tests: [
            function(helper) { helper.remove('/a', FileError.INVALID_MODIFICATION_ERR); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/c',}
        ],
    },
];

