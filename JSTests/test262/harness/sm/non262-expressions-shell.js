/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*---
defines: [testDestructuringArrayDefault, formatArray, toSource]
allow_unused: True
---*/

(function(global) {
    function func() {
    }
    class C {
        foo() {
        }
        static foo() {
        }
    }

    function test_one(pattern, val, opt) {
        var stmts = [];
        var i = 0;
        var c;

        stmts.push(`var ${pattern} = ${val};`);

        for (var stmt of stmts) {
            if (!opt.no_plain) {
                eval(`
${stmt}
`);
            }

            if (!opt.no_func) {
                eval(`
function f${i}() {
  ${stmt}
}
f${i}();
`);
                i++;

                eval(`
var f${i} = function foo() {
  ${stmt}
};
f${i}();
`);
                i++;

                eval(`
var f${i} = () => {
  ${stmt}
};
f${i}();
`);
                i++;
            }

            if (!opt.no_gen) {
                eval(`
function* g${i}() {
  ${stmt}
}
[...g${i}()];
`);
                i++;

                eval(`
var g${i} = function* foo() {
  ${stmt}
};
[...g${i}()];
`);
                i++;
            }

            if (!opt.no_ctor) {
                eval(`
class D${i} {
  constructor() {
    ${stmt}
  }
}
new D${i}();
`);
                i++;
            }

            if (!opt.no_derived_ctor) {
                if (opt.no_pre_super) {
                    eval(`
class D${i} extends C {
  constructor() {
    ${stmt}
    try { super(); } catch (e) {}
  }
}
new D${i}();
`);
                    i++;
                } else {
                    eval(`
class D${i} extends C {
  constructor() {
    super();
    ${stmt}
  }
}
new D${i}();
`);
                    i++;
                }
            }

            if (!opt.no_method) {
                eval(`
class D${i} extends C {
  method() {
    ${stmt}
  }
  static staticMethod() {
    ${stmt}
  }
}
new D${i}().method();
D${i}.staticMethod();
`);
                i++;
            }
        }

        if (!opt.no_func_arg) {
            eval(`
function f${i}(${pattern}) {}
f${i}(${val});
`);
            i++;

            eval(`
var f${i} = function foo(${pattern}) {};
f${i}(${val});
`);
            i++;

            eval(`
var f${i} = (${pattern}) => {};
f${i}(${val});
`);
            i++;
        }

        if (!opt.no_gen_arg) {
            eval(`
function* g${i}(${pattern}) {}
[...g${i}(${val})];
`);
            i++;

            eval(`
var g${i} = function* foo(${pattern}) {};
[...g${i}(${val})];
`);
            i++;
        }
    }

    function test(expr, opt={}) {
        var pattern = `[a=${expr}, ...c]`;
        test_one(pattern, "[]", opt);
        test_one(pattern, "[1]", opt);

        pattern = `[,a=${expr}]`;
        test_one(pattern, "[]", opt);
        test_one(pattern, "[1]", opt);
        test_one(pattern, "[1, 2]", opt);

        pattern = `[{x: [a=${expr}]}]`;
        test_one(pattern, "[{x: [1]}]", opt);

        pattern = `[x=[a=${expr}]=[]]`;
        test_one(pattern, "[]", opt);
        test_one(pattern, "[1]", opt);

        pattern = `[x=[a=${expr}]=[1]]`;
        test_one(pattern, "[]", opt);
        test_one(pattern, "[1]", opt);
    }

    global.testDestructuringArrayDefault = test;
})(this);

(function(global) {
  /*
   * Date: 07 February 2001
   *
   * Functionality common to Array testing -
   */
  //-----------------------------------------------------------------------------


  var CHAR_LBRACKET = '[';
  var CHAR_RBRACKET = ']';
  var CHAR_QT_DBL = '"';
  var CHAR_COMMA = ',';
  var CHAR_SPACE = ' ';
  var TYPE_STRING = typeof 'abc';


  /*
   * If available, arr.toSource() gives more detail than arr.toString()
   *
   * var arr = Array(1,2,'3');
   *
   * arr.toSource()
   * [1, 2, "3"]
   *
   * arr.toString()
   * 1,2,3
   *
   * But toSource() doesn't exist in Rhino, so use our own imitation, below -
   *
   */
  function formatArray(arr)
  {
    try
    {
      return arr.toSource();
    }
    catch(e)
    {
      return toSource(arr);
    }
  }

  global.formatArray = formatArray;

  /*
   * Imitate SpiderMonkey's arr.toSource() method:
   *
   * a) Double-quote each array element that is of string type
   * b) Represent |undefined| and |null| by empty strings
   * c) Delimit elements by a comma + single space
   * d) Do not add delimiter at the end UNLESS the last element is |undefined|
   * e) Add square brackets to the beginning and end of the string
   */
  function toSource(arr)
  {
    var delim = CHAR_COMMA + CHAR_SPACE;
    var elt = '';
    var ret = '';
    var len = arr.length;

    for (i=0; i<len; i++)
    {
      elt = arr[i];

      switch(true)
      {
	case (typeof elt === TYPE_STRING) :
	ret += doubleQuote(elt);
	break;

	case (elt === undefined || elt === null) :
	break; // add nothing but the delimiter, below -

	default:
	ret += elt.toString();
      }

      if ((i < len-1) || (elt === undefined))
	ret += delim;
    }

    return  CHAR_LBRACKET + ret + CHAR_RBRACKET;
  }

  global.toSource = toSource;

  function doubleQuote(text)
  {
    return CHAR_QT_DBL + text + CHAR_QT_DBL;
  }
})(this);
