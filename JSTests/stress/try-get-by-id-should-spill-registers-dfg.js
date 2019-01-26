var createBuiltin = $vm.createBuiltin;

let f = createBuiltin(`(function (arg) { 
    let r = @tryGetById(arg, "prototype");
    if (arg !== true) throw new Error("Bad clobber of arg");
    return r;
})`);
noInline(f);

for (let i = 0; i < 10000; i++) {
    f(true);
}
