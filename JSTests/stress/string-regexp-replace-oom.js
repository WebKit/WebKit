//@ skip if $memoryLimited or $addressBits <= 32
//@ runDefault()

Object.__proto__.__proto__[Symbol.replace] = () => {};

function __f_0(__v_5, __v_6) {
  while (__v_5.length < __v_6) {
    __v_5 += __v_5;
  }
  return __v_5.substring(0, __v_6);
}

var __v_2 = __f_0("1", 1 << 20);
var __v_3 = __f_0("$1", 1 << 16);
var flag = false;
try {
  __v_2.replace(/(.+)/g, __v_3);
} catch(e) {
  flag = true;
}

if(!flag) throw new Error("expected an exception");
