class API_Boolean {
    constructor(value) {
        this.value = value;
    }

    encode() {
        return {
            optionalValue: {
                subclasses: {
                    variantType: "API::Boolean",
                    variant: {
                        value: this.value
                    }
                }
            }
        }
    }
}

class API_String {
    constructor(str) {
        this.str = str;
    }

    encode() {
        return {
            optionalValue: {
                subclasses: {
                    variantType: "API::String",
                    variant: {
                        string: this.str
                    }
                }
            }
        }
    }
}

class API_Array {
    constructor(elements) {
        this.elements = elements;
    }

    encode() {
        return {
            optionalValue: {
                subclasses: {
                    variantType: "API::Array",
                    variant: {
                        elements: this.elements
                    }
                }
            }
        }
    }
}

class API_Dictionary {
    constructor(values) {
        this.values = values;
    }

    encode() {
        return {
            optionalValue: {
                subclasses: {
                    variantType: "API::Dictionary",
                    variant: {
                        map: this.values
                    }
                }
            }
        }
    }
}

class API_UInt64 {
    constructor(value) {
        this.value = value;
    }

    encode() {
        return {
            optionalValue: {
                subclasses: {
                    variantType: "API::UInt64",
                    variant: {
                        value: this.value
                    }
                }
            }
        }
    }
}

class NSString {
    constructor(str) {
        this.str = str;
    }

    encode() {
        return new API_Dictionary([
            { key: '$class', value: new API_String("NSString").encode() },
            { key: '$string', value: new API_String(this.str).encode() }
        ]).encode();
    }
}

class NSNumber {
    constructor(value) {
        this.value = value;
    }

    encode() {
        return new API_Dictionary([
            { key: '$class', value: new API_String("NSNumber").encode() },
            // see     frame #0: 0x0000000184f8fc04 Foundation`-[NSPlaceholderNumber initWithCoder:](self=0x00000001ea154ca0, _cmd=<unavailable>, decoder=0x00006000027c8040) at NSValue.m:2158:13 [opt]
            { key: 'NS.intval', value: new API_UInt64(this.value).encode() }
        ]).encode();
    }
}

class NSInvocation {
    constructor(selector, typeString, isReplyBlock=false) {
        this.selector = selector;
        this.typeString = typeString;
        this.isReplyBlock = isReplyBlock;
    }
    encode() {
        return new API_Dictionary([
            { key: '$class', value: new API_String("NSInvocation").encode() },
            { key: 'selector', value: new NSString(this.selector).encode() },
            { key: 'typeString', value: new NSString(this.typeString).encode() },
            { key: 'isReplyBlock', value: new API_Boolean(this.isReplyBlock).encode() }
        ]).encode()
    }
}
