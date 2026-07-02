//@ runDefault("--watchdog=2", "--watchdog-exception-ok")

class C {
  #field;

  constructor() {
    gc();
    #field in this;
  }
}

for (let i = 0; i < 1000; i++) {
  new C();
}
