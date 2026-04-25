class Hello {
    *gen() {
        yield;
    }

    static *gen() {
        yield;
    }

    *get() {
    }

    static *get() {
    }

    *set() {
    }

    static *set() {
    }

    *"Hello"() {
    }

    static *"Hello"() {
    }

    *20() {
    }

    static *20() {
    }

    *[42]() {
    }

    static *[42]() {
    }
}

let object = {
    *gen() {
        yield;
    },

    *get() {
    },

    *set() {
    },

    *"Hello"() {
    },

    *20() {
    },

    *[42]() {
    }
}
