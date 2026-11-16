function fn() {
  return '𠮷'.match(/^.$/u);
}

assertEqual(!!fn(), true);
test(fn);
