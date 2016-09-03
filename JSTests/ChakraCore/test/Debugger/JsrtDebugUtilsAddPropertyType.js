//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj = {
  a1 : undefined,
  a2 : null,
  a3 : false,
  a4 : new Boolean(true),
  a5 : 1,
  a6 : 1.2,
  a7 : NaN,
  a8 : Infinity,
  a9 : -Infinity,
  a10 : 0,
  a11 : -0,
  a12 : "Hello World!",
  a13 : Symbol('symbol'),
  a14 : function func() {},
  a15 : [],
  a16 : [1, 2, 3],
  a17 : [1.1, 2.2, 3.3],
  a18 : {},
  a19 : new RegExp('Hello World!'),
  a20 : new Error('Hello World!'),
  a21 : new TypeError('Hello World!'),
  a22 : new String('Hello World!'),
  a23 : new Number(100),
  a24 : new Number(1.1),
  a25 : new Map(),
  a26 : new Set(),
  a27 : new WeakMap(),
  a28 : new WeakSet(),
  a29 : new ArrayBuffer(10),
  a30 : new Int8Array(1),
  a31 : new Uint8Array(1),
  a32 : new Uint8ClampedArray(1),
  a33 : new Int16Array(1),
  a34 : new Uint16Array(1),
  a35 : new Int32Array(1),
  a36 : new Uint32Array(1),
  a37 : new Float32Array(1),
  a38 : new Float64Array(1),
  a39 : new DataView(new ArrayBuffer(10))
}

obj; /**bp:evaluate('obj', 2);**/

var target = {};
var handler = {};
var obj2 = {
  a1 : new Proxy(target, handler)
};

obj2; /**bp:evaluate('obj2.a1', 2);**/
obj2; /**bp:evaluate('obj2.a2 = 1');**/
WScript.Echo("pass");
