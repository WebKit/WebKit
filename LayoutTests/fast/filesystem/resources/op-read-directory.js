var testCases = [
    {
        name: 'ReadDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b', isDirectory:true},
            {fullPath:'/c', },
            {fullPath:'/d', },
            {fullPath:'/e', isDirectory:true},
            {fullPath:'/f', },
            {fullPath:'/g', isDirectory:true},
            {fullPath:'/a/b'},
            {fullPath:'/a/c'},
        ],
        tests: [
            function(helper) { helper.readDirectory('/'); },
            function(helper) { helper.remove('/c'); },
            function(helper) { helper.remove('/e'); },
            function(helper) { helper.remove('/f'); },
            function(helper) { helper.readDirectory('/'); }
        ],
    },
];
