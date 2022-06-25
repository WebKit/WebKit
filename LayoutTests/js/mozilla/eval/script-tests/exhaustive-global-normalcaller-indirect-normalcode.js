// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 514568;
var summary = "eval in all its myriad flavors";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var x = 17;

var ev = eval;

var xcode =
  "var x = 4;" +
  "function actX(action)" +
  "{" +
  "  switch (action)" +
  "  {" +
  "    case 'get':" +
  "      return x;" +
  "    case 'set1':" +
  "      x = 9;" +
  "      return;" +
  "    case 'set2':" +
  "      x = 23;" +
  "      return;" +
  "    case 'delete':" +
  "      try { return eval('delete x'); }" +
  "      catch (e) { return e.name; }" +
  "  }" +
  "}" +
  "actX;";

var local0 = x;

var f = ev(xcode);

var inner1 = f("get");
var local1 = x;

x = 7;
var inner2 = f("get");
var local2 = x;

f("set1");
var inner3 = f("get");
var local3 = x;

var del = f("delete");
var inner4 = f("get");
var local4 = x;

f("set2");
var inner5 = f("get");
var local5 = x;

var resultsX =
  {
     local0: local0,
     inner1: inner1, local1: local1,
     inner2: inner2, local2: local2,
     inner3: inner3, local3: local3,
     del: del,
     inner4: inner4, local4: local4,
     inner5: inner5, local5: local5,
   };

assertEq(resultsX.local0, 17);

assertEq(resultsX.inner1, 4);
assertEq(resultsX.local1, 4);

assertEq(resultsX.inner2, 7);
assertEq(resultsX.local2, 7);

assertEq(resultsX.inner3, 9);
assertEq(resultsX.local3, 9);

assertEq(resultsX.del, false);

assertEq(resultsX.inner4, 9);
assertEq(resultsX.local4, 9);

assertEq(resultsX.inner5, 23);
assertEq(resultsX.local5, 23);


var ycode =
  "var y = 5;" +
  "function actY(action)" +
  "{" +
  "  switch (action)" +
  "  {" +
  "    case 'get':" +
  "      return y;" +
  "    case 'set1':" +
  "      y = 2;" +
  "      return;" +
  "    case 'set2':" +
  "      y = 71;" +
  "      return;" +
  "    case 'delete':" +
  "      try { return eval('delete y'); }" +
  "      catch (e) { return e.name; }" +
  "  }" +
  "}" +
  "actY;";

try { var local0 = y; } catch (e) { local0 = e.name; }

var f = ev(ycode);

var inner1 = f("get");
var local1 = y;

y = 8;
var inner2 = f("get");
var local2 = y;

f("set1");
var inner3 = f("get");
var local3 = y;

var del = f("delete");
try { var inner4 = f("get"); } catch (e) { inner4 = e.name; }
try { var local4 = y; } catch (e) { local4 = e.name; }

f("set2");
try { var inner5 = f("get"); } catch (e) { inner5 = e.name; }
try { var local5 = y; } catch (e) { local5 = e.name; }

var resultsY =
  {
    local0: local0,
    inner1: inner1, local1: local1,
    inner2: inner2, local2: local2,
    inner3: inner3, local3: local3,
    del: del,
    inner4: inner4, local4: local4,
    inner5: inner5, local5: local5,
  };

assertEq(resultsY.local0, "ReferenceError");

assertEq(resultsY.inner1, 5);
assertEq(resultsY.local1, 5);

assertEq(resultsY.inner2, 8);
assertEq(resultsY.local2, 8);

assertEq(resultsY.inner3, 2);
assertEq(resultsY.local3, 2);

assertEq(resultsY.del, true);

assertEq(resultsY.inner4, "ReferenceError");
assertEq(resultsY.local4, "ReferenceError");

assertEq(resultsY.inner5, 71);
assertEq(resultsY.local5, 71);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete!");

var successfullyParsed = true;
