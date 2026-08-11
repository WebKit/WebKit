function test1() {
  function fn6(obj) {
    let array = new Array(4);
    array[0] = obj[1];
    var test = {
      f: array[1] ? array : 0
    };
    return obj;
  }
}
noInline(test1);

function test2() {
  function fn6(obj) {
    let array = new Array(4);
    array[0] = obj[1];
    var test = {
      f: array[1] ? array : 0
    };
    return obj;
  }
}
noInline(test2);

function test3() {
  function fn6(obj) {
    let array = new Array(4);
    array[0] = obj[1];
    var test = {
      f: array[1] ? array : 0
    };
    return obj;
  }
}
noInline(test3);

let obj1 = { 1: 1 };
let obj2 = { 1: 1.4 };
let obj3 = { 1: { } };
for (let i = 0; i < testLoopCount; ++i) {
    test1(obj1);
    test2(obj2);
    test3(obj3);
}

