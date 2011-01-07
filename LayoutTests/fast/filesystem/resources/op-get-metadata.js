var testCases = [
    {
        name: 'GetMetadata',
        precondition: [
            {fullPath:'/tmp'},
        ],
        tests: [
            function(helper) { helper.getMetadata('/'); },
            function(helper) { helper.getDirectory('/', '/a', {create:true}); },
            function(helper) { helper.getMetadata('/a'); },
            function(helper) { helper.getFile('/', '/b', {create:true}); },
            function(helper) { helper.getMetadata('/b'); },
            function(helper) { helper.remove('/tmp'); },
            function(helper) { helper.getMetadata('/tmp', FileError.NOT_FOUND_ERR); },
            function(helper) { helper.shouldBeGreaterThanOrEqual('/a.modificationTime', '/.modificationTime'); },
            function(helper) { helper.shouldBeGreaterThanOrEqual('/b.modificationTime', '/.modificationTime'); },
            function(helper) { helper.shouldBeGreaterThanOrEqual('/b.modificationTime', '/a.modificationTime'); }
        ],
        postcondition: [ ],
    },
];
