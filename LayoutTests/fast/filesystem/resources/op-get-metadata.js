var testCases = [
    {
        name: 'GetMetadata',
        precondition: [
            {fullPath:'/tmp'},
            {fullPath:'/file1', size:0},
            {fullPath:'/file2', size:10},
            {fullPath:'/file3', size:90},
        ],
        tests: [
            function(helper) { helper.getMetadata('/'); },
            function(helper) { helper.getDirectory('/', '/a', {create:true}); },
            function(helper) { helper.getMetadata('/a'); },
            function(helper) { helper.getMetadata('/file1'); },
            function(helper) { helper.getMetadata('/file2'); },
            function(helper) { helper.getMetadata('/file3'); },
            function(helper) { helper.getFile('/', '/b', {create:true}); },
            function(helper) { helper.getMetadata('/b'); },
            function(helper) { helper.remove('/tmp'); },
            function(helper) { helper.getMetadata('/tmp', FileError.NOT_FOUND_ERR); },
            function(helper) { helper.shouldBeGreaterThanOrEqual('/a.returned.modificationTime', '/.returned.modificationTime'); },
            function(helper) { helper.shouldBeGreaterThanOrEqual('/b.returned.modificationTime', '/.returned.modificationTime'); },
            function(helper) { helper.shouldBeGreaterThanOrEqual('/b.returned.modificationTime', '/a.returned.modificationTime'); }
        ],
        postcondition: [ ],
    },
];
