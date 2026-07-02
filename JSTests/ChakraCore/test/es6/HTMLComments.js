//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/* NOTE: This file needs to be treated as binary. It contains mixed line endings, including non-standard
 *       line endings. Most text editors will not handle the file correctly. If you need to edit this
 *       file, make sure you do a binary compare to ensure the non-standard line endings have not been lost.
 *
 *       'LS' refers to Unicode Character 'LINE SEPARATOR' (U+2028)
 *       'PS' refers to Unicode Character 'PARAGRAPH SEPARATOR' (U+2029)
 */

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

/*
 * Line terminator sequences - standard (11.3 LineTerminator)
 */
 
// CRLF
WScript.Echo("Code before CRLF--> is reachable");
--> WScript.Echo("Code after CRLF--> is unreachable");

// CR
WScript.Echo("Code before CR--> is reachable");
--> WScript.Echo("Code after CR--> is unreachable");

// LF
WScript.Echo("Code before LF--> is reachable");
--> WScript.Echo("Code after LF--> is unreachable");

// LS
WScript.Echo("Code before LS--> is reachable"); --> WScript.Echo("Code after LS--> is unreachable");

// PS
WScript.Echo("Code before PS--> is reachable"); --> WScript.Echo("Code after PS--> is unreachable");

/*
 * Line terminator sequences - non-standard (11.3 LineTerminatorSequence <CR>[lookahead != <LF>])
 */

// CRLS
WScript.Echo("Code before CRLS--> is reachable");
 --> WScript.Echo("Code after CRLS--> is unreachable");

// CRPS
WScript.Echo("Code before CRPS--> is reachable");
 --> WScript.Echo("Code after CRPS--> is unreachable");

// HTML open comment comments out the rest of the line
WScript.Echo("Code before <!-- is reachable"); <!-- WScript.Echo("Code after <!-- is unreachable");
WScript.Echo("Code before <!-- --> is reachable"); <!-- --> WScript.Echo("Code after <!-- --> is unreachable");

// Split multiline HTML comment comments out both lines
WScript.Echo("Code before <!-- LineTerminator --> is reachable"); <!-- WScript.Echo("Code after multiline <!-- is unreachable");
--> WScript.Echo("Code after <!-- LineTerminator --> is unreachable");

// Delimited comments syntax
/* Multi
   Line
   Comment */ --> WScript.Echo("Code after */ --> is unreachable");
WScript.Echo("Code before /* */ --> is reachable"); /* Comment */ --> WScript.Echo("Code after /* */ --> is unreachable");
WScript.Echo("Code before /* */--> is reachable"); /* Comment */--> WScript.Echo("Code after /* */--> is unreachable"); // No WhiteSpaceSequence

// Post-decrement with a greater-than comparison does not get interpreted as a comment
var a = 1; a-->a; WScript.Echo("Code after post-decrement with a greater-than comparison (-->) is reachable");
assert.areEqual(0, a, "Post decrement executes");

assert.throws(function () { eval('/* */ --->'); }, SyntaxError, "HTMLCloseComment causes syntax error with an extra -", "Syntax error");
