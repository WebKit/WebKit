//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function* makeValueGen(a, b) {
  // return a for profiling
  yield a;
  // return b to bailout
  yield b;
  // return b again to compare with non jit function
  yield b;
}

function* makeStartGen() {
  yield 0; // for interpreter
  yield 32; // for jitted version
  yield 32; // for jitted version
}

const global = this;
function RunMemsetTest(tests, arrayTypes, allTypes) {
  function makeTest(name, config) {
    const f1 = `function ${name}(arr) {
      for(var i = 0; i < 15; ++i) {arr[i] = ${config.stringValue};}
      return arr;
    }`;
    const f2 = customName => `function ${customName}P(arr, v) {
      for(var i = 1; i < 8; ++i) {arr[i] = v;}
      return arr;
    }`;
    const f3 = `function ${name}V(arr) {
      const v = ${config.stringValue};
      for(var i = -2; i < 17; ++i) {arr[i] = v;}
      return arr;
    }`;
    const f4 = customName => `function ${customName}Z(arr, start) {
      const v = ${config.stringValue};
      for(var i = start; i < 5; ++i) {arr[i] = v;}
      return arr;
    }`;

    const extraTests = allTypes.map((diffType, i) => {
      const difValue = {f: f2(`${name}W${i}`), compare: f2(`${name}WC${i}`)};
      const genValue = makeValueGen(config.stringValue, diffType);
      Reflect.defineProperty(difValue, "v", {
        get: () => genValue.next().value
      });
      return difValue;
    });

    const negativeLengthTest = {f: f4(name), compare: f4(`${name}C`), newForCompare: true};
    const genIndex = makeStartGen();
    Reflect.defineProperty(negativeLengthTest, "v", {
      get: () => genIndex.next().value
    });

    const tests = [
      {f: f1},
      {f: f2(name), v: config.v2 !== undefined ? config.v2 : config.stringValue},
      {f: f3},
      negativeLengthTest
    ].concat(extraTests);

    const convertTest = function(fnText) {
      var fn;
      eval(`fn = ${fnText}`);
      return fn;
    };
    for(const t of tests) {
      t.f = convertTest(t.f);
      t.compare = t.compare && convertTest(t.compare);
    }
    return tests;
  }


  let passed = true;
  for(const test of tests) {
    for(const t of arrayTypes) {
      const set1 = makeTest(`${test.name}${t}`, test);
      const testSets = [{
        set: set1,
        getArray() {return new global[t](10);}
      }].concat(allTypes.map((diffType, iType) => {
        return {
          set: makeTest(`${test.name}${t}${iType}`, test).slice(0, 1),
          getArray() {const arr = new global[t](10); for(let i = 0; i < 10; ++i) arr[i] = diffType; return arr;}
        };
      }));
      for(const testSet of testSets) {
        for(const detail of testSet.set) {
          const fn = detail.f;
          try {
            let a1 = fn(testSet.getArray(), detail.v);
            const a2 = fn(testSet.getArray(), detail.v);
            if(detail.compare) {
              // the optimized version ran with a different value. Run again with a clean function to compare=
              a1 = detail.compare(detail.newForCompare ? testSet.getArray() : a1, detail.v);
            }
            if(a1.length !== a2.length) {
              passed = false;
              print(`${fn.name} (${t}) didn't return arrays with same length`);
              continue;
            }
            for(let i = 0; i < a1.length; ++i) {
              if(a1[i] !== a2[i] && !(isNaN(a1[i]) && isNaN(a2[i]))) {
                passed = false;
                print(`${fn.name} (${t}): a1[${i}](${a1[i]}) != a2[${i}](${a2[i]})`);
                break;
              }
            }
          } catch(e) {
            if (e.message.indexOf("Number Expected") !== -1) {
              print(e.message);
              passed = false;
            }
          }
        }
      }
    }
  }
  return passed;
}
