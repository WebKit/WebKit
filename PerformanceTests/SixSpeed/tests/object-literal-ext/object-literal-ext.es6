function fn() {
  var name = 'foo';
  return {
    'bizz buzz'() {
      return 1;
    },
    name,
    [name]: 'bar',
    [name + 'foo']: 'foo'
  };
}

assertEqual(fn().foofoo, 'foo');
test(fn);
