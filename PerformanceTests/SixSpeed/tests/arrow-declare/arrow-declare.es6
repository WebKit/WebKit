
var obj = {
  value: 42,
  fn: function() {
    return () => this.value;
  }
};

assertEqual(obj.fn()(), 42);

test(function() {
  obj.fn();
});
