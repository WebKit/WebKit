let v0 = /(?:ab)?/gsu;
const v14 = {
    o(a3) {
        function f4() {
            return f4;
        }
        let v5 = 129;
        ({"flags":v5,"source":f4,...v0} = v0);
        function f7() {
            const v8 = /[z-\d]/gsy;
            const v9 = { __proto__: v8 };
            const t8 = v9.constructor;
            const v11 = new t8(v8, v5);
            v11.g = v11;
            return v9;
        }
        const v12 = new Uint32Array(1686);
        const v13 = v12.reduceRight(f7);
        v13.f = v13;
        return f7;
    },
};
try { v14.o(1686); } catch (e) {}
v14.o();
