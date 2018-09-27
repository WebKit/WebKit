//@ defaultNoNoLLIntRun if $architecture == "arm"

let args = new Array(0x10000);
args.fill();
args = args.map((_, i) => 'a' + i).join(', ');

let gun = eval(`(function () {
    class A {

    }

    class B extends A {
        constructor(${args}) {
            () => {
                ${args};
                super();
            };

            class C {
                constructor() {
                }

                trigger() {
                    (() => {
                        super.x;
                    })();
                }

                triggerWithRestParameters(...args) {
                    (() => {
                        super.x;
                    })();
                }
            }

            return new C();
        }
    }

    return new B();
})()`);

for (let i = 0; i < 0x10000; i++) {
    gun.trigger();
    gun.triggerWithRestParameters(1, 2, 3);
}
