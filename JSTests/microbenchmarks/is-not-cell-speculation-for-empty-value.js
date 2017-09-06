class Foo extends Object {
    constructor() {
        super();
        let arrow = () => {
            this.foo = 20;
        };
        this.arrow = arrow;
    }
}
noInline(Foo);

for (let i = 0; i < 400000; ++i)
    new Foo();
