function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main(){
  class a{
    g = []
    'a'(){}
  }
  shouldBe(Array.isArray(new a().g), true);
  shouldBe(new a().a(), undefined);
}
main();
