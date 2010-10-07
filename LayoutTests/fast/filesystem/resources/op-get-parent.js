var testCases = [
    {
        name: 'GetParent',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c'},
        ],
        tests: [
            function(helper) { helper.getParent('/'); },
            function(helper) { helper.getParent('/a'); },
            function(helper) { helper.getParent('/a/b'); },
            function(helper) { helper.getParent('/a/b/c'); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/a/b/c'},
        ],
    },
];
