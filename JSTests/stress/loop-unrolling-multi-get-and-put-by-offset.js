//@ runDefault("--forceEagerCompilation=true")
function foo() {
  const v0 = [];
  let v1 = 0;

  do {
      for (let v5 = v1; v5 < 7; v5++) {
          v0.c **= v5;
      }
      v1++;
  } while (v1 < 9);

  return v0;
}

for(let i = 0; i < 32; i++) {
    foo(); 
}
