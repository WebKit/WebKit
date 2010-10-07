var testCases = [
    {
        name: 'RestrictedChars',
        precondition: [
            {fullPath:'/a', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.getFile('/', 'con', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'CON', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'Con', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'cOn.txt', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a/coN', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            function(helper) { helper.getFile('/', 'prn', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'PRN', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'Prn', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'pRn.txt', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a/prN', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            function(helper) { helper.getFile('/', 'aux', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'AUX', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'Aux', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'aUx.txt', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a/auX', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            function(helper) { helper.getFile('/', 'nul', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'NUL', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'Nul', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'nUl.txt', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'a/nuL', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            function(helper) { helper.getFile('/', 'com1', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'COM2', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'Com4', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'cOM7.foo', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'b/coM9', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            function(helper) { helper.getFile('/', 'lpt1', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'LPT2', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'Lpt4', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'lPT7.foo', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'b/lpT9', {create:true}, FileError.INVALID_MODIFICATION_ERR); },

            // Names should not end with period or whitespace.
            function(helper) { helper.getFile('/', 'foo ', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'foo\n', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.getFile('/', 'foo.', {create:true}, FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/a', '/', 'foo ', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/a', '/', 'foo\t', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.copy('/a', '/', 'foo.', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.move('/a', '/', 'foo ', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.move('/a', '/', 'foo\t', FileError.INVALID_MODIFICATION_ERR); },
            function(helper) { helper.move('/a', '/', 'foo.', FileError.INVALID_MODIFICATION_ERR); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true}
        ],
    },
];
