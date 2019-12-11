var theParent = function () { };

function test1() {
    var base = class C extends theParent {
        static getParentStaticValue() {
            let arrow = (a,b,c) => super.getStaticValue(a,b,c);
            return arrow(1,1,1);
        }
    };

    for (let i = 0; i < 10000; i++) {
        try { base.getParentStaticValue()  } catch (e) {}
        try { base.getParentStaticValue()  } catch (e) {}
    }
}
test1();

function test2() {
    var base = class C extends theParent {
        static getParentStaticValue() {
            let arrow = () => super.getStaticValue();
            return arrow();
        }
    };

    for (let i = 0; i < 10000; i++) {
        try { base.getParentStaticValue()  } catch (e) {}
        try { base.getParentStaticValue()  } catch (e) {}
    }
}
test2();
