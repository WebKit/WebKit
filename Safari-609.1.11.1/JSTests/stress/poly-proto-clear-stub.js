function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}

function foo() {
    class C {
        constructor() {
            this.p0 = 0
            this.p1 = 1
            this.p2 = 2
            this.p3 = 3
            this.p4 = 4
            this.p5 = 5
            this.p6 = 6
            this.p7 = 7
            this.p8 = 8
            this.p9 = 9
            this.p10 = 10
            this.p11 = 11
            this.p12 = 12
            this.p13 = 13
            this.p14 = 14
            this.p15 = 15
            this.p16 = 16
            this.p17 = 17
            this.p18 = 18
            this.p19 = 19
            this.p20 = 20
            this.p21 = 21
            this.p22 = 22
            this.p23 = 23
            this.p24 = 24
            this.p25 = 25
            this.p26 = 26
            this.p27 = 27
            this.p28 = 28
            this.p29 = 29
            this.p30 = 30
            this.p31 = 31
            this.p32 = 32
            this.p33 = 33
            this.p34 = 34
            this.p35 = 35
            this.p36 = 36
            this.p37 = 37
            this.p38 = 38
            this.p39 = 39
        }
    };

    let item = new C;
    for (let i = 0; i < 20; ++i) {
        assert(item.p0 === 0)
        assert(item.p1 === 1)
        assert(item.p2 === 2)
        assert(item.p3 === 3)
        assert(item.p4 === 4)
        assert(item.p5 === 5)
        assert(item.p6 === 6)
        assert(item.p7 === 7)
        assert(item.p8 === 8)
        assert(item.p9 === 9)
        assert(item.p10 === 10)
        assert(item.p11 === 11)
        assert(item.p12 === 12)
        assert(item.p13 === 13)
        assert(item.p14 === 14)
        assert(item.p15 === 15)
        assert(item.p16 === 16)
        assert(item.p17 === 17)
        assert(item.p18 === 18)
        assert(item.p19 === 19)
        assert(item.p20 === 20)
        assert(item.p21 === 21)
        assert(item.p22 === 22)
        assert(item.p23 === 23)
        assert(item.p24 === 24)
        assert(item.p25 === 25)
        assert(item.p26 === 26)
        assert(item.p27 === 27)
        assert(item.p28 === 28)
        assert(item.p29 === 29)
        assert(item.p30 === 30)
        assert(item.p31 === 31)
        assert(item.p32 === 32)
        assert(item.p33 === 33)
        assert(item.p34 === 34)
        assert(item.p35 === 35)
        assert(item.p36 === 36)
        assert(item.p37 === 37)
        assert(item.p38 === 38)
        assert(item.p39 === 39)
    }
}

let start = Date.now();
for (let i = 0; i < 1000; ++i)
    foo();
if (false)
    print(Date.now() - start);
