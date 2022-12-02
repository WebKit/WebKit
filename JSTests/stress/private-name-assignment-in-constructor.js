//@ $skipModes << :lockdown if $buildType == "debug"

class Foo {
  #foo;
  constructor() {
    this.#foo /= 1;
  }
}
for (let i = 0; i < 10000000; ++i)
  new Foo();
