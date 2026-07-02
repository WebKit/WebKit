//@ $skipModes << :lockdown
"use strict"
const consoleStub = console || {
  // log: print
  log: () => {}
}
var console = consoleStub

function testForOf(arr) {
    let sum = 0|0
    for (const item of arr) {
        sum = ((sum|0) + (item|0))|0
    }
    return sum
}
noInline(testForOf)

function testForEach(arr) {
    let sum = 0|0
    arr.forEach(item => sum = ((sum|0) + (item|0))|0)
    return sum
}
noInline(testForEach)

// const warmup = 10000
const warmup = 1000
// const test = 5000
const test = 100
let run = 1

let arr = Array.from({length: 1}, (_, i) => i);

function runTest() {
  let resultsTotal = []
  let resultsForOf = []
  let resultsForEach = []
  
  for (let i = 0; i < test; ++i) {
      {
          let start = performance.now()
          for (let i = 0; i < run; i++) {
            testForOf(arr)
            testForEach(arr)
          }
          resultsTotal.push(performance.now() - start)
      }
  
      {
          let start = performance.now()
          for (let i = 0; i < run; i++) {
            testForEach(arr)
          }
          resultsForEach.push(performance.now() - start)
      }
  
      {
          let start = performance.now()
          for (let i = 0; i < run; i++) {
            testForOf(arr)
          }
          resultsForOf.push(performance.now() - start)
      }
  }
  return [
    resultsTotal.reduce((a, b) => a + b, 0) / resultsTotal.length,
    resultsForOf.reduce((a, b) => a + b, 0) / resultsForOf.length,
    resultsForEach.reduce((a, b) => a + b, 0) / resultsForEach.length
  ]
}
noInline(runTest)

for (let i = 0; i < warmup; i++) {
  runTest()
  run = (run + i) % 3 // make sure it isn't a constant
  arr = Array.from({length: (run + i) % 3}, (_, i) => i);
}
run = 10
arr = Array.from({length: 100000}, (_, i) => i);

console.log("Warmup complete")

let [resultsTotal, resultsForOf, resultsForEach] = runTest()

console.log("Total:", resultsTotal)
console.log("ForOf:", resultsForOf)
console.log("ForEach:", resultsForEach)
