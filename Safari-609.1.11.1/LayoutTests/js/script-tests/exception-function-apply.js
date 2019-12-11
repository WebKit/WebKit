description("Test for error messages on function.apply");

shouldThrow("function foo(){}; foo.apply(null, 20)");
shouldThrow("function foo(){}; foo.apply(null, 'hello')");
shouldThrow("function foo(){}; foo.apply(null, true)");
