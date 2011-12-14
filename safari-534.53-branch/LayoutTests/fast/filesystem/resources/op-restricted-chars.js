var testCases = [
    {
        name: 'RestrictedChars',
        precondition: [ ],
        tests: [
            // Restricted character in 'path' parameter.
            function(helper) { helper.getFile('/', 'a\\b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a<b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a>b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a:b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a?b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a*b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a"b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a|b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            function(helper) { helper.getFile('/', '\\ab', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', '<ab', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', ':ab', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', '?ab', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', '*ab', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', '"ab', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', '|ab', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            function(helper) { helper.getFile('/', 'ab\\', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'ab<', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'ab:', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'ab?', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'ab*', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'ab"', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'ab|', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            // This should succeed.
            function(helper) { helper.getFile('/', 'ab', {create:true}); },

            // Restricted character in 'name' parameters.
            function(helper) { helper.copy('/ab', '/', 'a\\b',FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/ab', '/', 'a<b', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/ab', '/', 'a:b', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/ab', '/', 'a?b', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/ab', '/', 'a*b', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/ab', '/', 'a"b', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/ab', '/', 'a|b', FileError.INVALID_MODIFICATION_ERR); },

            // 'Name' parameter cannot contain '/'.
            function(helper) { helper.copy('/ab', '/', 'a/b', FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/ab'},
        ],
    },
];
