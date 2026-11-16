class Foo {
    constructor(o, value) {
        try {
            throw { o: 1 };
        } catch({ o }){
            if (o !== 1)
                throw new Error();
            o = 2;
        }
        if (o !== value)
            throw new Error();

        try {
            throw [ 1 ];
        } catch([ o ]){
            if (o !== 1)
                throw new Error();
            o = 2;
        }
        if (o !== value)
            throw new Error();

    }
}
new Foo("string", "string");


class Bar {
    constructor(value) {
        let o = value;
        try {
            throw { o: 1 };
        } catch({ o }){
            if (o !== 1)
                throw new Error();
            o = 2;
        }
        if (o !== value)
            throw new Error();

        try {
            throw [ 1 ];
        } catch([ o ]){
            if (o !== 1)
                throw new Error();
            o = 2;
        }
        if (o !== value)
            throw new Error();

    }
}
new Bar("string");

class Baz {
    constructor(value) {
        const o = value;
        try {
            throw { o: 1 };
        } catch({ o }){
            if (o !== 1)
                throw new Error();
            o = 2;
        }
        if (o !== value)
            throw new Error();

        try {
            throw [ 1 ];
        } catch([ o ]){
            if (o !== 1)
                throw new Error();
            o = 2;
        }
        if (o !== value)
            throw new Error();

    }
}
new Baz("string");
