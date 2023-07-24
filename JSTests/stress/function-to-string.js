function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe((function test() { }).toString(), `function test() { }`);
shouldBe((() => { }).toString(), `() => { }`);
shouldBe((function* test() { }).toString(), `function* test() { }`);
shouldBe((async function* test() { }).toString(), `async function* test() { }`);
shouldBe((async function test() { }).toString(), `async function test() { }`);
shouldBe((async () => { }).toString(), `async () => { }`);
shouldBe((function foo2   () {}).toString(), "function foo2   () {}");
shouldBe((function       foo3()   {}).toString(), "function       foo3()   {}");
shouldBe((function*  fooGen   (){}).toString(), "function*  fooGen   (){}");
shouldBe((async function fnasync() {}).toString(), "async function fnasync() {}");

let f1 = async function() {}
shouldBe(f1.toString(),"async function() {}");

let f2 = async()=>{}
shouldBe(f2.toString(), "async()=>{}");

let f3 = async  ()    =>     {}
shouldBe(f3.toString(), "async  ()    =>     {}");

let asyncGenExpr = async function*()  {}
shouldBe(asyncGenExpr.toString(), "async function*()  {}");

async function* asyncGenDecl() {}
shouldBe(asyncGenDecl.toString(), "async function* asyncGenDecl() {}");

async  function  *  fga  (  x  ,  y  )  {  ;  ;  }
shouldBe(fga.toString(),"async  function  *  fga  (  x  ,  y  )  {  ;  ;  }");

let fDeclAndExpr = { async f  (  )  {  } }.f;
shouldBe(fDeclAndExpr.toString(),"async f  (  )  {  }");

let fAsyncGenInStaticMethod = class { static async  *  f  (  )  {  } }.f;
shouldBe(fAsyncGenInStaticMethod.toString(),"async  *  f  (  )  {  }");

let fPropFuncGen = { *  f  (  )  {  } }.f;
shouldBe(fPropFuncGen.toString(),"*  f  (  )  {  }");

let fGetter = Object.getOwnPropertyDescriptor(class { static get  f  (  )  {  } }, "f").get;
shouldBe(fGetter.toString(),"get  f  (  )  {  }");

let g = class { static [  "g"  ]  (  )  {  } }.g;
shouldBe(g.toString(),'[  "g"  ]  (  )  {  }');

let fMethodObject = { f  (  )  {  } }.f;
shouldBe(fMethodObject.toString(),"f  (  )  {  }");

let fComputedProp = { [  "f"  ]  (  )  {  } }.f;
shouldBe(fComputedProp.toString(),'[  "f"  ]  (  )  {  }');

let gAsyncPropFunc = { async  [  "g"  ]  (  )  {  } }.g;
shouldBe(gAsyncPropFunc.toString(),'async  [  "g"  ]  (  )  {  }');
