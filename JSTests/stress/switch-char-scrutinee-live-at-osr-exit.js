//@ requireOptions("--useLLInt=0", "--forceEagerCompilation=1", "--poisonDeadOSRExitVariables=1")

function opt(a4) {
    switch (a4) {
    case 'a':
        function f() { a4 }
    case 'b':
    case 'c':
    }
}

opt('');
opt([]);
