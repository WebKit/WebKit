class U {
    constructor() {
        return Object;
    }
}

for (let i = 0; i < 1000; i++) {
    class C extends U {
        #x;
        #y() {}
        constructor() {
            super();
        }
    }
    new C();
    gc();
}
