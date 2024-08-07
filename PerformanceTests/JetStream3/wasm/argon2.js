
function ok(v) {
    if (v)
        return;
    throw new Error("not ok");
}

function deepStrictEqual(buf1, buf2) {
    if (buf1.byteLength != buf2.byteLength) return false;
    var dv1 = new Int8Array(buf1);
    var dv2 = new Int8Array(buf2);
    for (var i = 0; i != buf1.byteLength; i++) {
        if (dv1[i] != dv2[i])
            throw new Error("deepStrictEqual failed");
    }
}

function strictEqual(a, b) {
    if (a !== b)
        throw new Error("strictEqual failed");
}

function match(str, regex) {
    if (!str.match(regex))
        throw new Error("match failed");
}

globalThis.instantiateArgon2WasmInstance = async function () {
    await argon2.loadModule();
};

// NOTE: the Argon2 hash test is based on
// https://github.com/antelle/argon2-browser/blob/master/test/suite/hash.js
globalThis.testArgon2Hash = async function () {
    {
        let hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
        });
        ok(hash);
        deepStrictEqual(
            hash.hash,
            new Uint8Array([36, 99, 163, 35, 223, 30, 71, 14, 26, 134, 8, 54, 243, 110, 116, 23, 61, 129, 40, 65, 101, 227, 197, 230])
        );
        strictEqual(
            hash.hashHex,
            '2463a323df1e470e1a860836f36e74173d81284165e3c5e6'
        );
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm'
        );
    }

    {
        let hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
        });
        ok(hash);
        strictEqual(
            hash.hashHex,
            '2463a323df1e470e1a860836f36e74173d81284165e3c5e6'
        );
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm'
        );

        hash = await argon2.hash({
            pass: 'p@ssw0rd2',
            salt: 'somesalt',
        });
        ok(hash);
        strictEqual(
            hash.hashHex,
            'f02360855fe22752362451bcd41206041304ce7908b5645b'
        );
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$8CNghV/iJ1I2JFG81BIGBBMEznkItWRb'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            type: argon2.ArgonType.Argon2d,
        });
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            type: argon2.ArgonType.Argon2i,
        });
        strictEqual(
            hash.encoded,
            '$argon2i$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$I4odzyBxoIvAnqjao5xt4/xS02zts+Jb'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            type: argon2.ArgonType.Argon2id,
        });
        strictEqual(
            hash.encoded,
            '$argon2id$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$dF4LZGqnznPWATUG/6m1Yp/Id1nKVSlk'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            time: 10,
        });
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=10,p=1$c29tZXNhbHQ$uo6bzQ2Wjb9LhxGoSHnaMrIOAB/6dGAS'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            mem: 2048,
        });
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=2048,t=1,p=1$c29tZXNhbHQ$6OXzaE6bdwMuoUqJt5U1TCrfJBF/8X3O'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            hashLen: 32,
        });
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$3w1C/UXK5b+K9eGlhctsuw1TivVU1JFCmB+DmM+MIiY'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            parallelism: 4,
        });
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=4$c29tZXNhbHQ$0/nLmgLRIKqzzOET6/YYHgZrp0xJjACg'
        );
    }

    {
        const hash = await argon2.hash({
            pass: '汉字漢字',
            salt: 'asdfasdfasdfasdf',
            type: argon2.ArgonType.Argon2id,
            hashLen: 32,
        });
        strictEqual(
            hash.encoded,
            '$argon2id$v=19$m=1024,t=1,p=1$YXNkZmFzZGZhc2RmYXNkZg$zzqgQLEjqikDwII1Qk2ZbyoCG12D25W7tXSgejiwiS0'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            ad: new Uint8Array([1, 2, 3]),
        });
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$CuATwqimHlFHq0mTT0qnozY4kCjrGg7X'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            secret: new Uint8Array([1, 2, 3]),
        });
        strictEqual(
            hash.encoded,
            '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$kPOHk/DlE6du1nkbKKom8FV+fcNjviLW'
        );
    }

    {
        const hash = await argon2.hash({
            pass: 'p@ssw0rd',
            salt: 'somesalt',
            hashLen: 100000,
        });
        match(
            hash.encoded,
            /^\$argon2d\$v=19\$m=1024,t=1,p=1\$c29tZXNhbHQ\$A0myEnizYlgEZQ7gUkfNESi8\/.*\/2Zy0G7fzjEl\/bKA$/
        );
    }
};

// NOTE: the Argon2 verify test is based on
// https://github.com/antelle/argon2-browser/blob/master/test/suite/verify.js
globalThis.testArgon2Verify = async function () {
    {
        let error = undefined;
        await argon2
            .verify({
                pass: 'p@ssw0rd2',
                encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm',
            })
            .catch((e) => (error = e));
        ok(error);
        ok(error.code);
        strictEqual(
            error.message,
            'The password does not match the supplied hash'
        );
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm',
        });
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm',
            type: argon2.ArgonType.Argon2d,
        });
    }

    {
        let error = undefined;
        await argon2
            .verify({
                pass: 'p@ssw0rd',
                encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$JGOjI98eRw4ahgg28250Fz2BKEFl48Xm',
                type: argon2.ArgonType.Argon2i,
            })
            .catch((e) => (error = e));
        ok(error);
        strictEqual(error.message, 'Decoding failed');
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2i$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$I4odzyBxoIvAnqjao5xt4/xS02zts+Jb',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2id$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$dF4LZGqnznPWATUG/6m1Yp/Id1nKVSlk',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2d$v=19$m=1024,t=10,p=1$c29tZXNhbHQ$uo6bzQ2Wjb9LhxGoSHnaMrIOAB/6dGAS',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2d$v=19$m=2048,t=1,p=1$c29tZXNhbHQ$6OXzaE6bdwMuoUqJt5U1TCrfJBF/8X3O',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$3w1C/UXK5b+K9eGlhctsuw1TivVU1JFCmB+DmM+MIiY',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            encoded: '$argon2d$v=19$m=1024,t=1,p=4$c29tZXNhbHQ$0/nLmgLRIKqzzOET6/YYHgZrp0xJjACg',
        });
    }

    {
        await argon2.verify({
            pass: '汉字漢字',
            encoded: '$argon2id$v=19$m=1024,t=1,p=1$YXNkZmFzZGZhc2RmYXNkZg$zzqgQLEjqikDwII1Qk2ZbyoCG12D25W7tXSgejiwiS0',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            ad: new Uint8Array([1, 2, 3]),
            encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$CuATwqimHlFHq0mTT0qnozY4kCjrGg7X',
        });
    }

    {
        await argon2.verify({
            pass: 'p@ssw0rd',
            secret: new Uint8Array([1, 2, 3]),
            encoded: '$argon2d$v=19$m=1024,t=1,p=1$c29tZXNhbHQ$kPOHk/DlE6du1nkbKKom8FV+fcNjviLW',
        });
    }
};



globalThis.testArgon2HashAndVerify = async function () {
    await testArgon2Hash();
    await testArgon2Verify();

    let randomStrings = ['iqqdAsbeeb', 'B3tVB5h2AI', 'Z7ihQxGE93', 'ou2jsFcbur', '0pjTjkrnsM', 'B8VRY9tCVY', '6Kg3AmWnVc', '3jcEFqDAPT', 'JeehIKnWiO', '2mSSPZacYX', 'CGRhFvbj3M', 'aQPgDC7e5j', 'bSUE8duQ4u', 'Xp3FEiAEXi', 'tusLv0aHkH', '4GUC7Hx4yk', 'yJIS8ioQLH', 'OmS9931mEz', 'ekSwTfKgvC', '3kj2W1fy9F', '1oXMME4xPV', 'UDDI3kCDqy', 'xglo5DvJm3', '2SWNQygbHm', 'Msfw59Go9X', 'X9vvXuoDM0', 'XUvEPoHvXC', 'pffHsNtWBg', 'EQPLyqD9U5', 'yBpDjiKHHj', '2gStCWSzhD', 'k74wcxwve7', 'TU2BIQTMqE', 'bVaCWRL4l3', 'BMTlPabHpu', 'KXv8hmWApM', 'lpvttDdRli', 'M4WB62T2qy', 'FbSoElUBC0', 'QpsrcOC4C1'];

    async function hashAndVerify(params) {
        for (let i = 0; i < randomStrings.length; i += 2) {
            params.pass += randomStrings[i];
            params.salt += randomStrings[i + 1];
            let hash = await argon2.hash(params);
            await argon2.verify({
                pass: params.pass,
                encoded: hash.encoded,
                type: hash.type,
                ad: params.ad,
                secret: params.secret
            }).catch(e => { throw new Error(e.message) });
        }
    }

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        type: argon2.ArgonType.Argon2d,
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        type: argon2.ArgonType.Argon2i,
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        type: argon2.ArgonType.Argon2id,
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        time: 10,
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        mem: 2048,
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        hashLen: 32,
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        parallelism: 4,
    });

    await hashAndVerify({
        pass: '汉字漢字',
        salt: 'asdfasdfasdfasdf',
        type: argon2.ArgonType.Argon2id,
        hashLen: 32,
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        ad: new Uint8Array([1, 2, 3]),
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        secret: new Uint8Array([1, 2, 3]),
    });

    await hashAndVerify({
        pass: 'p@ssw0rd',
        salt: 'somesalt',
        hashLen: 100000,
    });
};