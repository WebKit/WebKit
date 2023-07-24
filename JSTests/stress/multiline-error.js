function foo() {
  return RegExp;
}

class C extends foo {
  constructor() {
    super();
    super.multiline = undefined;
  }
}

try {
    new C();
} catch { }
