function inline() {}
function named() {
    var x;
}
function outer1() {
    function inner() {
        var y;
    }
}
function outer2()
{
    function inner()
    {
        var y;
    }
}
function outer3() {
    var x;
    function inner() { var y; }
}
function outer4() {
    function inner() { var y; }
    var x;
}
function outer5() {
    var x;
    function inner() { var y; }
    var x;
}
function outer6() {
    function inner1() { var y; }
    var x;
    function inner2() { var z; }
}
function outer7() {
    function inner1() { var y; }
    function inner2() { var z; }
    var x;
}
function outer7() {
    function inner1() { var y; }
    function inner2() { var z; }
}

(function() {
    var x;
})();

(() => {
    var x;
});

function* generator() {
    var x;
}

class Class {
    static staticMethod1() {
        var x;
    }
    static staticMethod2()
    {
        var x;
    }
    method1() {
        var x;
    }
    method2()
    {
        var x;
    }
    get getter1() {
        var x;
    }
    get getter2()
    {
        var x;
    }
    set setter1(x) {
        var s;
    }
    set setter2(x)
    {
        var s;
    }
}

x => x;
() => 1;
(x) => x;
(x) => { x };
(x) => {
    x
};
() => {
    var x;
};

var fObj = {
    f1: function() {
        var x;
    },
    f2: function()
    {
        var x;
    },
    f3: () => {
        var x;
    },
    f4: () =>
    {
        var x;
    }   
};
