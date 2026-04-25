//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



var str = "abbbacdefgabbba";

WScript.Echo(str.lastIndexOf("bba"));
WScript.Echo(str.lastIndexOf("bba", str.length - 1));
WScript.Echo(str.lastIndexOf("bba", str.length - 2));
WScript.Echo(str.lastIndexOf("bba", str.length - 3));
WScript.Echo(str.lastIndexOf("bba", str.length - 4));
WScript.Echo(str.lastIndexOf("xyz"));
WScript.Echo(str.lastIndexOf("fgb"));
WScript.Echo(str.lastIndexOf("edca"));
WScript.Echo(str.lastIndexOf("acde"));
WScript.Echo(str.lastIndexOf(""));
WScript.Echo(str.lastIndexOf("", 4));

var str2 = "\0abcd\0\0";
WScript.Echo(str2.lastIndexOf("\0\0"));
WScript.Echo(str2.lastIndexOf("cd\0"));
WScript.Echo(str2.lastIndexOf("\0ab"));

var str3 = "\u0100\u0111\u0112\u0113";
WScript.Echo(str3.lastIndexOf("\u0112\u0113"));

var str4 = "\u0061\u0062\u0063\u3042\u3044";
WScript.Echo(str4.lastIndexOf("\u3042\u3044", 1));
WScript.Echo(str4.lastIndexOf("\u3042\u3044"));
WScript.Echo(str4.lastIndexOf("\u3042\u3044", 4));
WScript.Echo("\u3042\u3044\u0061".lastIndexOf("\u3042\u3044", 3));
WScript.Echo(str4.lastIndexOf("\u3044\0", 5));

//implicit calls
var a = 1;
var b = 2;
var obj = {toString: function(){ a=3; return "Hello World";}};
a = b;
Object.prototype.concat = String.prototype.concat;
var f = obj.concat("abc");
WScript.Echo (a);