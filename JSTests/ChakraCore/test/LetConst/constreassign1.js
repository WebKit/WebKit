//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

const a = 1;

with({a:2}) {
    a++;
    print(a);  // 3
}

try {
    with({b:2}) {
        a++;
        print(a);
    }
}
catch(e) {
    print(e);  // TypeError: Assignment to const
}

let foo1 = new Function(
    "with({a:2}) {" + 
    "    a++;" +
    "    print(a);" +
    "}");

foo1();   // 3

let foo2 = new Function(
    "with({b:2}) {" + 
    "    a++;" +
    "    print(a);" +
    "}");

try {
    foo2();
}
catch(e) {
    print(e);  // TypeError: Assignment to const
}

try {
    eval('let b = 3');
    a++;
    print(a);
}
catch(e) {
    print(e);  // TypeError: Assignment to const
}

(function() {
    const a = 1;
    with({a:2}) {
        a++;
        print(a);  // 3
    }

    try {
        with({b:2}) {
            a++;
            print(a);
        }
    }
    catch(e) {
        print(e);  // TypeError: Assignment to const
    }

    try {
        eval('let b = 3');
        a++;
        print(a);
    }
    catch(e) {
        print(e);  // TypeError: Assignment to const
    }
})();

function print(x) { WScript.Echo(x) }

