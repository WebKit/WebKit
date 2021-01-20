//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.WScript.LoadScriptFile(".\\memset_tester.js");

const allTypes = [0, 1.5, undefined, null, 9223372036854775807, "string", {a: null, b: "b"}];

const tests = [
  {name: "memsetUndefined", stringValue: undefined},
  {name: "memsetNull", stringValue: null},
  {name: "memsetInt", stringValue: 0, v2: 1 << 30},
  {name: "memsetFloat", stringValue: 3.14, v2: -87.684},
  {name: "memsetNumber", stringValue: 9223372036854775807, v2: -987654987654987},
  {name: "memsetBoolean", stringValue: true, v2: false},
  {name: "memsetString", stringValue: "\"thatString\"", v2: "`A template string`"},
  {name: "memsetObject", stringValue: "{test: 1}", v2: [1, 2, 3]},
];

const types = "Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array Float32Array Float64Array Array".split(" ");

let passed = RunMemsetTest(tests, types, allTypes);

function memsetSymbol() {const s = Symbol(); const arr = new Array(10); for(let i = 0; i < 10; ++i) {arr[i] = s;} return arr;}
function memsetSymbolV(v) {const arr = new Array(10); for(let i = 0; i < 10; ++i) {arr[i] = v;} return arr;}
function checkSymbols() {
  const s = Symbol();
  // Since symbol are unique, and we want to compare the result, we have to pass the same symbol everytime
  const a1 = memsetSymbolV(s);
  const a2 = memsetSymbolV(s);
  for(let i = 0; i < a1.length; ++i) {
    if(a1[i] !== a2[i]) {
      passed = false;
      // need explicit toString() for Symbol
      print(`memsetSymbolV: a1[${i}](${a1[i].toString()}) != a2[${i}](${a2 && a2[i].toString() || ""})`);
      break;
    }
  }

  memsetSymbol();
  const symbolArray = memsetSymbol();
  for(let i = 0; i < symbolArray.length; ++i) {
    if(typeof symbolArray[i] !== typeof s) {
      passed = false;
      print(`memsetSymbol: symbolArray[${i}] is not a Symbol`);
      break;
    }
  }
}
checkSymbols();

print(passed ? "PASSED" : "FAILED");
