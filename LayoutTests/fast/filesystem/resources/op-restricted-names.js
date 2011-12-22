var testCases = [
    {
        name: 'RestrictedNames',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b', isDirectory:true},
            {fullPath:'/c', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.getFile('/', '.', {create:true}, FileError.SECURITY_ERR); },
            function(helper) { helper.getFile('/', '..', {create:true}, FileError.SECURITY_ERR); },
            function(helper) { helper.getFile('/', 'con', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'CON', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'Con', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'cOn.txt', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a/coN', {create:true}, 0); },

            function(helper) { helper.getFile('/', 'prn', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'PRN', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'Prn', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'pRn.txt', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a/prN', {create:true}, 0); },

            function(helper) { helper.getFile('/', 'aux', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'AUX', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'Aux', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'aUx.txt', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a/auX', {create:true}, 0); },

            function(helper) { helper.getFile('/', 'nul', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'NUL', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'Nul', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'nUl.txt', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a/nuL', {create:true}, 0); },

            function(helper) { helper.getFile('/', 'com1', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'COM2', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'Com4', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'cOM7.foo', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a/coM9', {create:true}, 0); },

            function(helper) { helper.getFile('/', 'lpt1', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'LPT2', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'Lpt4', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'lPT7.foo', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'a/lpT9', {create:true}, 0); },

            function(helper) { helper.getFile('/', 'foo ', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'foo\n', {create:true}, 0); },
            function(helper) { helper.getFile('/', 'foo.', {create:true}, 0); },
            function(helper) { helper.copy('/a', '/', 'foo  ', 0); },
            function(helper) { helper.copy('/a', '/', 'foo\t', 0); },
            function(helper) { helper.copy('/a', '/', 'foo..', 0); },
            function(helper) { helper.move('/a', '/', 'foo    ', 0); },
            function(helper) { helper.move('/b', '/', 'foo\t\t', 0); },
            function(helper) { helper.move('/c', '/', 'foo.....', 0); },
        ],
        postcondition: [
            {fullPath:'/foo    ', isDirectory:true},
            {fullPath:'/foo\t\t', isDirectory:true},
            {fullPath:'/foo.....', isDirectory:true}
        ],
    },
];
