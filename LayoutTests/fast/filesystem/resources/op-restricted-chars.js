var testCases = [
    {
        name: 'RestrictedChars',
        precondition: [
            {fullPath:'/ab', isDirectory:true},
        ],
        tests: [
            // Test for difficult characters in 'path' parameters.
            function(helper) { helper.getFile('/', 'a<b', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a>b', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a:b', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a?b', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a*b', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a"b', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a|b', {create:true}, 0); },

            function(helper) { helper.getFile('/', '<ab', {create:true}, 0); },
            function(helper) { helper.getFile('/', ':ab', {create:true}, 0); },
            function(helper) { helper.getFile('/', '?ab', {create:true}, 0); },
            function(helper) { helper.getFile('/', '*ab', {create:true}, 0); },
            function(helper) { helper.getFile('/', '"ab', {create:true}, 0); },
            function(helper) { helper.getFile('/', '|ab', {create:true}, 0); },

            function(helper) { helper.getFile('/', 'ab<', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'ab:', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'ab?', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'ab*', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'ab"', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'ab|', {create:true}, 0); },

            // Only '\\' is disallowed.
            function(helper) { helper.getFile('/', 'a\\b', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            // Test for difficult characters in 'name' parameters.
            function(helper) { helper.copy('/ab', '/', ' a<b', 0); },
            function(helper) { helper.copy('/ab', '/', ' a:b', 0); },
            function(helper) { helper.copy('/ab', '/', ' a?b', 0); },
            function(helper) { helper.copy('/ab', '/', ' a*b', 0); },
            function(helper) { helper.copy('/ab', '/', ' a"b', 0); },
            function(helper) { helper.copy('/ab', '/', ' a|b', 0); },

            // 'Name' parameter cannot contain '/' or '\\'.
            function(helper) { helper.copy('/ab', '/', 'a/b', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/ab', '/', 'a\\b', FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/ab', isDirectory:true},
            {fullPath:'a<b', isDirectory:false},
            {fullPath:' a|b', isDirectory:true}
        ],
    },
];
