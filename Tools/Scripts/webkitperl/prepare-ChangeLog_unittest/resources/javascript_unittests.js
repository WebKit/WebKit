/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

function func1()
{
}

function func2() {
}

function func3() { }

function func4()
{
    return 123;
}

function func5()
{
    // comment
}

function func6()
{
    /* comment */
}

/*
function funcInsideComment()
{
}
*/

function func7()
{
}

var str1 = "function funcInsideDoubleQuotedString() {}";

function func8()
{
}

var str2 = 'function funcInsideSingleQuotedString() {}';

function func9(a)
{
}

function func10(a, b)
{
}

function func11
    (a, b)
{
}

function func12(a, b,
                c, d
                , e, f)
{
}

function funcOverloaded()
{
}

function funcOverloaded(a)
{
}

function funcOverloaded(a, b)
{
}

Func1.prototype = {
    get x1()
    {
    },

    get x2()
    {
        return this.x2;
    },

    set x1(a)
    {
    },

    set x3(a)
    {
        this.x3 = a;
    }
};

Func2.prototype = {
    func13 : function()
    {
    },

    func14 : function(a)
    {
    },

    func15 : function(a, b)
    {
        return 123;
    }
};

function func16()
{
    var a = 123;
    var b = 456;

    var func17 = function() {
    };

    var func18 = function(a) {
    };

    var func19 = function(a, b) {
        return 123;
    };

    func20(function()
           {
           },
           function(a)
           {
               return 123;
           });
}

class BaseClass { constructor() {} };

class DerivedClass extends BaseClass {
    constructor()
    {
        super();
    }

    static staticMethod()
    {
    }

    method(a, b)
    {
        function nestedFunctionInsideMethod() {
            // ..
        }
    }

    get getter() { }
}

var namespace = {};
namespace.MyClass = class IgnoredName {
    constructor()
    {
    }

    static staticMethod()
    {
    }

    method(a, b)
    {
        function nestedFunctionInsideMethod() {
            // ..
        }
    }

    get getter() { }
}

Foo.prototype = {
    method1() { },

    method2(a)
    {
    },

    method3(a, b)
    {
        function innerFunction() {
            // ...
        }
        return innerFunction();
    }
};

(function(asdf) {
    function insideGlobalAnonymousFunctionExpression(obj)
    {
    }
})();

(function foo(asdf) {
    function insideGlobalFunctionExpression(obj)
    {
    }
})();

class IssueWithMapGetAndSet {
    method1()
    {
        this.map.set(a, b);
        let object = {property: value};
        function nestedFunctionInsideMethod1(){}
        this._callingSomething();
    }

    method2()
    {
        this.map.get(a);
    }

    method3()
    {
    }
};

class AsyncFuncClass {
    static async staticAsync()
    {
    }

    async methodAsync()
    {
    }

    async get getAsync()
    {
    }

    async set setAsync()
    {
    }
};

async function asyncFunc1()
{
}
