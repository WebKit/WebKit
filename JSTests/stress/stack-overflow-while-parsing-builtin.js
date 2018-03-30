function f() {
    try {
        f();
    } catch (e) {
        try {
            Map.prototype.forEach.call('', {});
        } catch {}
    }
}

f()
