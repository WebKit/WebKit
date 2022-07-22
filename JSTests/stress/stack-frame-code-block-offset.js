//@ skip if $memoryLimited
class __c_0 {
  constructor() { return __v_20 }
}
class __c_1 extends __c_0 {
  #x
}
try {
  __v_20 = __c_1
  try {
    for (;;)
      new __c_1
  } catch {
  }
} catch {
}
