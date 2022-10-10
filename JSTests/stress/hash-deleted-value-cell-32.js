//@ skip if $buildType != "debug" or ($architecture != "arm" and $architecture != "mips")
function __getProperties(obj) {
  properties = []
  for (name of Object.getOwnPropertyNames(obj)) properties.push(name)
}

function __getRandomProperty(obj) {
  __getProperties(obj)
  return properties
}

startSamplingProfiler()

__v_45 = {
  shouldThrow(__v_50) {
        __v_50()
  }
}

for (i = 0; i < 3e5; ++i)
  try {
    __v_45.shouldThrow(() => { Object.defineProperty(__v_45, __getRandomProperty(__v_45), {value : -1}) })
  } catch {
  }
