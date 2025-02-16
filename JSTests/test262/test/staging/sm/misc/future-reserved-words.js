/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 497869;
var summary = "Implement FutureReservedWords per-spec";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var futureReservedWords =
  [
   "class",
   // "const", // Mozilla extension enabled even for versionless code
   "enum",
   "export",
   "extends",
   "import",
   "super",
  ];

var strictFutureReservedWords =
  [
   "implements",
   "interface",
   "let", // enabled: this file doesn't execute as JS1.7
   "package",
   "private",
   "protected",
   "public",
   "static",
   "yield", // enabled: this file doesn't execute as JS1.7
  ];

function testWord(word, expectNormal, expectStrict)
{
  var actual, status;

  // USE AS LHS FOR ASSIGNMENT

  actual = "";
  status = summary + ": " + word + ": normal assignment";
  try
  {
    eval(word + " = 'foo';");
    actual = "no error";
  }
  catch(e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict assignment";
  try
  {
    eval("'use strict'; " + word + " = 'foo';");
    actual = "no error";
  }
  catch(e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS DESTRUCTURING SHORTHAND

  actual = "";
  status = summary + ": " + word + ": destructuring shorthand";
  try
  {
    eval("({ " + word + " } = 'foo');");
    actual = "no error";
  }
  catch(e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict destructuring shorthand";
  try
  {
    eval("'use strict'; ({ " + word + " } = 'foo');");
    actual = "no error";
  }
  catch(e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE IN VARIABLE DECLARATION

  actual = "";
  status = summary + ": " + word + ": normal var";
  try
  {
    eval("var " + word + ";");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict var";
  try
  {
    eval("'use strict'; var " + word + ";");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE IN FOR-IN VARIABLE DECLARATION

  actual = "";
  status = summary + ": " + word + ": normal for-in var";
  try
  {
    eval("for (var " + word + " in {});");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict for-in var";
  try
  {
    eval("'use strict'; for (var " + word + " in {});");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS CATCH IDENTIFIER

  actual = "";
  status = summary + ": " + word + ": normal var";
  try
  {
    eval("try { } catch (" + word + ") { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict var";
  try
  {
    eval("'use strict'; try { } catch (" + word + ") { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS LABEL

  actual = "";
  status = summary + ": " + word + ": normal label";
  try
  {
    eval(word + ": while (false);");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict label";
  try
  {
    eval("'use strict'; " + word + ": while (false);");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS ARGUMENT NAME IN FUNCTION DECLARATION

  actual = "";
  status = summary + ": " + word + ": normal function argument";
  try
  {
    eval("function foo(" + word + ") { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict function argument";
  try
  {
    eval("'use strict'; function foo(" + word + ") { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  actual = "";
  status = summary + ": " + word + ": function argument retroactively strict";
  try
  {
    eval("function foo(" + word + ") { 'use strict'; }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS ARGUMENT NAME IN FUNCTION EXPRESSION

  actual = "";
  status = summary + ": " + word + ": normal function expression argument";
  try
  {
    eval("var s = (function foo(" + word + ") { });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict function expression argument";
  try
  {
    eval("'use strict'; var s = (function foo(" + word + ") { });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  actual = "";
  status = summary + ": " + word + ": function expression argument retroactively strict";
  try
  {
    eval("var s = (function foo(" + word + ") { 'use strict'; });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS ARGUMENT NAME WITH FUNCTION CONSTRUCTOR

  actual = "";
  status = summary + ": " + word + ": argument with normal Function";
  try
  {
    Function(word, "return 17");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": argument with strict Function";
  try
  {
    Function(word, "'use strict'; return 17");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS ARGUMENT NAME IN PROPERTY SETTER

  actual = "";
  status = summary + ": " + word + ": normal property setter argument";
  try
  {
    eval("var o = { set x(" + word + ") { } };");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict property setter argument";
  try
  {
    eval("'use strict'; var o = { set x(" + word + ") { } };");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  actual = "";
  status = summary + ": " + word + ": property setter argument retroactively strict";
  try
  {
    eval("var o = { set x(" + word + ") { 'use strict'; } };");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS FUNCTION NAME IN FUNCTION DECLARATION

  actual = "";
  status = summary + ": " + word + ": normal function name";
  try
  {
    eval("function " + word + "() { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict function name";
  try
  {
    eval("'use strict'; function " + word + "() { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  actual = "";
  status = summary + ": " + word + ": function name retroactively strict";
  try
  {
    eval("function " + word + "() { 'use strict'; }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  // USE AS FUNCTION NAME IN FUNCTION EXPRESSION

  actual = "";
  status = summary + ": " + word + ": normal function expression name";
  try
  {
    eval("var s = (function " + word + "() { });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectNormal, status);

  actual = "";
  status = summary + ": " + word + ": strict function expression name";
  try
  {
    eval("'use strict'; var s = (function " + word + "() { });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);

  actual = "";
  status = summary + ": " + word + ": function expression name retroactively strict";
  try
  {
    eval("var s = (function " + word + "() { 'use strict'; });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  assert.sameValue(actual, expectStrict, status);
}

function testFutureReservedWord(word)
{
  testWord(word, "SyntaxError", "SyntaxError");
}

function testStrictFutureReservedWord(word)
{
  testWord(word, "no error", "SyntaxError");
}

futureReservedWords.forEach(testFutureReservedWord);
strictFutureReservedWords.forEach(testStrictFutureReservedWord);

/******************************************************************************/

print("All tests passed!");
