//@ runDefault("--forceEagerCompilation=true")
function foo(){
  const v1 = [0];
  let v2 = 0;

  while (v2 !== 2) {
      v1.a ||= v2;
      v2++;
  }

  return v1;
}


for(let i = 0; i < 32; i++) {
  foo(42);
}
