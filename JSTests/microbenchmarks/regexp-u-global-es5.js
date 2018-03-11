function fn() {
    return '𠮷'.match(/^.$/g);
}
noInline(fn);

for (var i = 0; i < 1e6; ++i)
    fn();
