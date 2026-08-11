//@ $skipModes << :lockdown
"use strict"
const consoleStub = console || {
  // log: print
  log: () => {}
}
var console = consoleStub

function testFor(arr) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i++) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}
noInline(testFor)

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

const warmup = 5000
const test = 20
let run = 1

const arr1 = [1, 2, 3, 4, 5]

function runTest() {
  let resultsTotal = []
  let resultsFor = []
  let resultsForOf = []
  let resultsForEach = []
  
  for (let i = 0; i < test; ++i) {
      {
          let start = performance.now()
          for (let i = 0; i < run; i++) {
            testFor(arr1)
            testForOf(arr1)
            testForEach(arr1)
          }
          resultsTotal.push(performance.now() - start)
      }
      {
          let start = performance.now()
          for (let i = 0; i < run; i++) {
            testFor(arr1)
          }
          resultsFor.push(performance.now() - start)
      }
      {
          let start = performance.now()
          for (let i = 0; i < run; i++) {
            testForEach(arr1)
          }
          resultsForEach.push(performance.now() - start)
      }
  
      {
          let start = performance.now()
          for (let i = 0; i < run; i++) {
            testForOf(arr1)
          }
          resultsForOf.push(performance.now() - start)
      }
  }
  return [
    resultsTotal.reduce((a, b) => a + b, 0) / resultsTotal.length,
    resultsFor.reduce((a, b) => a + b, 0) / resultsFor.length,
    resultsForOf.reduce((a, b) => a + b, 0) / resultsForOf.length,
    resultsForEach.reduce((a, b) => a + b, 0) / resultsForEach.length
  ]
}
noInline(runTest)

for (let i = 0; i < warmup; i++) {
  runTest()
  run = (run + i) % 10 // make sure it isn't a constant
}
// run = 1_000_000
run = 1000

console.log("Warmup complete")

let [resultsTotal, resultsFor, resultsForOf, resultsForEach] = runTest()

console.log("Total:", resultsTotal)
console.log("For:", resultsFor)
console.log("ForOf:", resultsForOf)
console.log("ForEach:", resultsForEach)
