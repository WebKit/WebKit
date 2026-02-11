//@runDefault("--useTestingHelpers=1", "--useConcurrentJIT=0", "--forceICFailure=1", "--useOSREntryToDFG=0", "--verboseFTLOSRExit=1")
"use strict";

const INT32_MAX = 0x7fffffff|0
const UINT32_MAX = (0xffffffff>>>0)

function launder(value) { return value }
noInline(launder)

function definitions(dis) {
  let defs = { }
  let children = { }
  let ops = { }
  dis.split("\n").forEach(line => {
    // D@79:<!2:loc15>	GetLocal(Check:Untyped:D@177, JS|MustGen|PureNum|NeedsNegZero|NeedsNaNOrInfinity|UseAsOther, Array, loc7(K<Array>/FlushedCell), machine:loc4, R:Stack(loc7), bc#43, ExitValid)  predicting Array
    let match = line.match(/D@(\d+):/)
    if (!match)
      return;
    let id = match[1].trim()
    defs[id] = line
    children[id] = line.matchAll(/(?:D@(\d+),)|(?:D@(\d+)\))/g).flatMap(m => [ m[1].trim() ])
    ops[id] = line.match(/D@\d+:.*>\s+([a-zA-Z0-9]+)\(/)[1].trim()
  })
  
  return { defs, children, ops,
    forEachOp: function(targetOp, callback) {
      for (let [id, op] of Object.entries(ops)) {
        if (op === targetOp)
          callback(id)
      }
    },
  }
}

function dfgHasBoundsCheck(defs) {
  // GetByVal(X) with a GetButterfly(X)
  let found = false

  defs.forEachOp("GetByVal", (id) => {
    if (found)
      return;
    for (let child of defs.children[id]) {
      if (defs.ops[child] === "GetButterfly") {
        found = true
        break
      }
    }
  })
  return found
}

function ftlHasBoundsCheck(defs) {
  let found = false
  defs.forEachOp("CheckInBounds", (id) => {
    found = true
  })
  // Alternatively, our GetByVal can do the check
  defs.forEachOp("GetByVal", (id) => {
    if (!defs.defs[id].includes("InBounds"))
      found = true
  })
  return found
}

let elidable = []
let unelidable = []
let afters = []

function canElide(fn, arg = 0) {
  noInline(fn)
  elidable.push({ fn, arg })
}

function cannotElide(fn, arg = 0) {
  noInline(fn)
  unelidable.push({ fn, arg })
}

function after(fn) {
  noInline(fn)
  afters.push({ fn })
}

// This tests that loop unrolling won't break IRO
canElide(function fnnnnn(arr) {
  let sum = 0|0
  for (let i = 0; i+3 < arr.length; i+=4) {
    sum = ((sum|0) + (arr[i+3]|0))|0
  }
  return sum - 8 - 4 + 36
})

/*
canElide(function (arr) {
    let sum = 0|0
    for (const item of arr) {
        sum = ((sum|0) + (item|0))|0
    }
    return sum
})

canElide(function (arr) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i++) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
})

canElide(function (arr) {
  let sum = 0|0
  for (let i = 0; (i|0) < (arr.length|0); i = (i + 1)|0) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
})

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i = (i + 1)|0) {
    if (arg === 1337 && i === -1)
      i = 0
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1338)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i = (i + 1)|0) {
    if (arg === 1337 && i === -1)
      i = 0
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1338)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = -1; i < arr.length; i = (i + 1)|0) {
    if (arg === 1337 && i === -1)
      i = 0
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i = (i + 1)|0) {
    if (arg === 1337 && i === -1)
      i = -1
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i = (i + 1)|0) {
    if (arg === 1337 && i === 8 && (500 < arr.length))
      i = 500
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i = (i + 1)|0) {
    if (arg === 1337 && i === 8 && ((INT32_MAX-1) < arr.length))
      i = (INT32_MAX-1)
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

// We never stop looping, but still don't go out of bounds
canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i = (i + 1)|0) {
    if (arg === 1337 && i === 8 && ((INT32_MAX) < arr.length))
      i = (INT32_MAX)
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length; i = i + 1) {
    if (arg === 1337 && i === 8 && ((INT32_MAX+1) < arr.length))
      i = (INT32_MAX+1)
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length - 1; i = (i + 1)) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum + 8
}, 1337)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length + 1; i = i + 1) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length + 1; i = (i + 1)|0) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < (arr.length + 1)|0; i = (i + 1)|0) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < ((arr.length)|0) && arr.length < INT32_MAX-1; i = (i + 1)|0) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < ((arr.length)|0); i = (i + 1)|0) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; i < arr.length - 1; i = (i + 1)) {
    sum = ((sum|0) + (arr[i+1]|0))|0
  }
  return sum + 1
}, 1337)

// Unsigned numbers
canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; (i>>>0) < ((arr.length)>>>0); i = ((i + 1)|0)) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; (i>>>0) < ((arr.length + 1)>>>0); i = ((i + 1)|0)) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; (i>>>0) < ((arr.length - 2)>>>0); i = ((i + 1)|0)) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum + 8 + 7
}, 1337)

cannotElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; (i>>>0) < ((arr.length - 1)>>>0); i = ((i + 1)|0)) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum + 8
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  for (let i = 0; (i>>>0) < ((arr.length)>>>0); i = ((i + 1))) {
    sum = ((sum|0) + (arr[i]|0))|0
  }
  return sum
}, 1337)

canElide(function(arr, arg) {
  let sum = 0|0
  let len = arr.length
  if (len === 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i]|0))|0
    i = i + 1
    if (i === len)
      break
  } while (true)
  return sum
}, 1337)

canElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length
  if (len <= 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i]|0))|0
    i = i + 1
    // This totally becomes negative if arr.length overflows, but the Int32Use guards against that.
    if ((i>>>0) === (len>>>0))
      break
  } while (true)
  return sum
}, 1337)

canElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length - 1
  if (len <= 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) === (len>>>0))
      break
  } while (true)
  return sum + 8
}, 1337)

cannotElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length + 1
  if (len <= 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) === (len>>>0))
      break
  } while (true)
  return sum
}, 1337)

cannotElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length
  if (len <= 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) === ((len - 1)>>>0))
      break
  } while (true)
  return sum + 8
}, 1337)

cannotElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length
  if (len <= 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) === ((len+1)>>>0))
      break
  } while (true)
  return sum
}, 1337)

cannotElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) === ((len)>>>0))
      break
  } while (true)
  return sum
}, 1337)

cannotElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length
  if (len < 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) === ((len)>>>0))
      break
  } while (true)
  return sum
}, 1337)

canElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length
  if (len <= 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) >= ((len)>>>0))
      break
  } while (true)
  return sum
}, 1337)

cannotElide(function (arr, arg) {
  let sum = 0|0
  let len = arr.length
  if (len <= 0)
    return sum
  let i = 0
  do {
    sum = ((sum|0) + (arr[i|0]|0))|0
    i = (i + 1)|0
    if ((i>>>0) > ((len)>>>0))
      break
  } while (true)
  return sum
}, 1337)
*/
function getDisassembly(fn, arg) {
  const arr1 = [1, 2, 3, 4, 5, 6, 7, 8].slice(0, 8)
  let dfg = ""
  let ftl = ""

  for (let i = 0; i < testLoopCount; ++i) {
    if (fn(arr1, arg) !== 36)
      throw -1
    try {
      dfg = $vm.getDFGDisassembly(fn)
    } catch (_) {
    }
  }
  ftl = $vm.getFTLDisassembly(fn)

  if (dfg.length === 0)
    throw -3

  if (ftl.length === 0)
    throw -4

  return { dfg, ftl }
}
noInline(getDisassembly)

let test = 0
for (let { fn, arg } of elidable) {
  print("*<TEST>*")
  print(fn, " is ", test)
  let { dfg, ftl } = getDisassembly(fn, arg)
  if (!dfgHasBoundsCheck(definitions(dfg)))
    throw test
  test++
  if (ftlHasBoundsCheck(definitions(ftl)))
     throw test
  test++
}

print("*<TEST>* Unelidable tests")

for (let { fn, arg } of unelidable) {
  print("*<TEST>*")
  print(fn, " is ", test)
  let { dfg, ftl } = getDisassembly(fn, arg)
  if (!dfgHasBoundsCheck(definitions(dfg)))
    throw test
  test++
  if (!ftlHasBoundsCheck(definitions(ftl)))
    throw test
  test++
}

// print("*<TEST>* After tests")

for (let { fn } of afters) {
  fn()
}
