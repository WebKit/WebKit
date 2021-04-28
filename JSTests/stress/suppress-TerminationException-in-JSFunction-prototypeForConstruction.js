//@ runDefault("--watchdog=300", "--watchdog-exception-ok")

class U {}

function foo() {
  class C extends U {
    constructor() {
      gc();
      super();
    }
  }
  new C();
}

while(1) foo();
