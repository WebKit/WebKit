function Fn() {
  return (new.target === Fn);
}

assertEqual(typeof Fn(), 'boolean');
assertEqual(typeof (new Fn()), 'object');

test(function() {
  return (Fn() || new Fn());
});
