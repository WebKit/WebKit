/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*---
defines: [testRegExp, clone_object_check]
allow_unused: True
---*/

(function(global) {
  /*
   * Date: 07 February 2001
   *
   * Functionality common to RegExp testing -
   */
  //-----------------------------------------------------------------------------

  var MSG_PATTERN = '\nregexp = ';
  var MSG_STRING = '\nstring = ';
  var MSG_EXPECT = '\nExpect: ';
  var MSG_ACTUAL = '\nActual: ';
  var ERR_LENGTH = '\nERROR !!! match arrays have different lengths:';
  var ERR_MATCH = '\nERROR !!! regexp failed to give expected match array:';
  var ERR_NO_MATCH = '\nERROR !!! regexp FAILED to match anything !!!';
  var ERR_UNEXP_MATCH = '\nERROR !!! regexp MATCHED when we expected it to fail !!!';
  var CHAR_LBRACKET = '[';
  var CHAR_RBRACKET = ']';
  var CHAR_QT_DBL = '"';
  var CHAR_QT = "'";
  var CHAR_NL = '\n';
  var CHAR_COMMA = ',';
  var CHAR_SPACE = ' ';
  var TYPE_STRING = typeof 'abc';



  function testRegExp(statuses, patterns, strings, actualmatches, expectedmatches)
  {
    var status = '';
    var pattern = new RegExp();
    var string = '';
    var actualmatch = new Array();
    var expectedmatch = new Array();
    var state = '';
    var lActual = -1;
    var lExpect = -1;


    for (var i=0; i != patterns.length; i++)
    {
      status = statuses[i];
      pattern = patterns[i];
      string = strings[i];
      actualmatch=actualmatches[i];
      expectedmatch=expectedmatches[i];
      state = getState(status, pattern, string);

      description = status;

      if(actualmatch)
      {
        actual = formatArray(actualmatch);
        if(expectedmatch)
        {
          // expectedmatch and actualmatch are arrays -
          lExpect = expectedmatch.length;
          lActual = actualmatch.length;

          var expected = formatArray(expectedmatch);

          if (lActual != lExpect)
          {
            reportCompare(lExpect, lActual,
                          state + ERR_LENGTH +
                          MSG_EXPECT + expected +
                          MSG_ACTUAL + actual +
                          CHAR_NL
	                       );
            continue;
          }

          // OK, the arrays have same length -
          if (expected != actual)
          {
            reportCompare(expected, actual,
                          state + ERR_MATCH +
                          MSG_EXPECT + expected +
                          MSG_ACTUAL + actual +
                          CHAR_NL
	                       );
          }
          else
          {
            reportCompare(expected, actual, state)
	        }

        }
        else //expectedmatch is null - that is, we did not expect a match -
        {
          expected = expectedmatch;
          reportCompare(expected, actual,
                        state + ERR_UNEXP_MATCH +
                        MSG_EXPECT + expectedmatch +
                        MSG_ACTUAL + actual +
                        CHAR_NL
	                     );
        }

      }
      else // actualmatch is null
      {
        if (expectedmatch)
        {
          actual = actualmatch;
          reportCompare(expected, actual,
                        state + ERR_NO_MATCH +
                        MSG_EXPECT + expectedmatch +
                        MSG_ACTUAL + actualmatch +
                        CHAR_NL
	                     );
        }
        else // we did not expect a match
        {
          // Being ultra-cautious. Presumably expectedmatch===actualmatch===null
          expected = expectedmatch;
          actual   = actualmatch;
          reportCompare (expectedmatch, actualmatch, state);
        }
      }
    }
  }

  global.testRegExp = testRegExp;

  function getState(status, pattern, string)
  {
    /*
     * Escape \n's, etc. to make them LITERAL in the presentation string.
     * We don't have to worry about this in |pattern|; such escaping is
     * done automatically by pattern.toString(), invoked implicitly below.
     *
     * One would like to simply do: string = string.replace(/(\s)/g, '\$1').
     * However, the backreference $1 is not a literal string value,
     * so this method doesn't work.
     *
     * Also tried string = string.replace(/(\s)/g, escape('$1'));
     * but this just inserts the escape of the literal '$1', i.e. '%241'.
     */
    string = string.replace(/\n/g, '\\n');
    string = string.replace(/\r/g, '\\r');
    string = string.replace(/\t/g, '\\t');
    string = string.replace(/\v/g, '\\v');
    string = string.replace(/\f/g, '\\f');

    return (status + MSG_PATTERN + pattern + MSG_STRING + singleQuote(string));
  }


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


  function doubleQuote(text)
  {
    return CHAR_QT_DBL + text + CHAR_QT_DBL;
  }


  function singleQuote(text)
  {
    return CHAR_QT + text + CHAR_QT;
 }

})(this);


(function(global) {

  // The Worker constructor can take a relative URL, but different test runners
  // run in different enough environments that it doesn't all just automatically
  // work. For the shell, we use just a filename; for the browser, see browser.js.
  var workerDir = '';

  // Assert that cloning b does the right thing as far as we can tell.
  // Caveat: getters in b must produce the same value each time they're
  // called. We may call them several times.
  //
  // If desc is provided, then the very first thing we do to b is clone it.
  // (The self-modifying object test counts on this.)
  //
  function clone_object_check(b, desc) {
    function classOf(obj) {
      return Object.prototype.toString.call(obj);
    }

    function ownProperties(obj) {
      return Object.getOwnPropertyNames(obj).
        map(function (p) { return [p, Object.getOwnPropertyDescriptor(obj, p)]; });
    }

    function isArrayLength(obj, pair) {
      return Array.isArray(obj) && pair[0] == "length";
    }

    function isCloneable(obj, pair) {
      return isArrayLength(obj, pair) || (typeof pair[0] === 'string' && pair[1].enumerable);
    }

    function notIndex(p) {
      var u = p >>> 0;
      return !("" + u == p && u != 0xffffffff);
    }

    function assertIsCloneOf(a, b, path) {
      assertEq(a === b, false);

      var ca = classOf(a);
      assertEq(ca, classOf(b), path);

      assertEq(Object.getPrototypeOf(a),
               ca == "[object Object]" ? Object.prototype : Array.prototype,
               path);

      // 'b', the original object, may have non-enumerable or XMLName
      // properties; ignore them (except .length, if it's an Array).
      // 'a', the clone, should not have any non-enumerable properties
      // (except .length, if it's an Array) or XMLName properties.
      var pb = ownProperties(b).filter(function(element) {
        return isCloneable(b, element);
      });
      var pa = ownProperties(a);
      for (var i = 0; i < pa.length; i++) {
        assertEq(typeof pa[i][0], "string", "clone should not have E4X properties " + path);
        if (!isCloneable(a, pa[i])) {
          throw new Error("non-cloneable clone property " + pa[i][0] + " " + path);
        }
      }

      // Check that, apart from properties whose names are array indexes, 
      // the enumerable properties appear in the same order.
      var aNames = pa.map(function (pair) { return pair[1]; }).filter(notIndex);
      var bNames = pa.map(function (pair) { return pair[1]; }).filter(notIndex);
      assertEq(aNames.join(","), bNames.join(","), path);

      // Check that the lists are the same when including array indexes.
      function byName(a, b) { a = a[0]; b = b[0]; return a < b ? -1 : a === b ? 0 : 1; }
      pa.sort(byName);
      pb.sort(byName);
      assertEq(pa.length, pb.length, "should see the same number of properties " + path);
      for (var i = 0; i < pa.length; i++) {
        var aName = pa[i][0];
        var bName = pb[i][0];
        assertEq(aName, bName, path);

        var path2 = path + "." + aName;
        var da = pa[i][1];
        var db = pb[i][1];
        if (!isArrayLength(a, pa[i])) {
          assertEq(da.configurable, true, path2);
        }
        assertEq(da.writable, true, path2);
        assertEq("value" in da, true, path2);
        var va = da.value;
        var vb = b[pb[i][0]];
        if (typeof va === "object" && va !== null)
          queue.push([va, vb, path2]);
        else
          assertEq(va, vb, path2);
      }
    }

    var banner = "while testing clone of " + (desc || JSON.stringify(b));
    var a = deserialize(serialize(b));
    var queue = [[a, b, banner]];
    while (queue.length) {
      var triple = queue.shift();
      assertIsCloneOf(triple[0], triple[1], triple[2]);
    }

    return a; // for further testing
  }
  global.clone_object_check = clone_object_check;

})(this);
