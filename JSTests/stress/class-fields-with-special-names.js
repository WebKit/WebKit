function assertEquals(e, a) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

{
  class A {
    async
    get
    test() { return "foo"; }
  }

  let a = new A();
  assertEquals(true, 'async' in a);
  assertEquals("foo", a.test);
}

{
  class A {
    super;
    static;
    set;
    get;
    test() { return "foo"; }
  }

  let a = new A();
  assertEquals(true, 'set' in a);
  assertEquals(true, 'get' in a);
  assertEquals(true, 'static' in a);
  assertEquals(true, 'super' in a);
  assertEquals("foo", a.test());
}

{
  class A {
    static = "test";
  }

  let a = new A();
  assertEquals("test", a.static);
}

