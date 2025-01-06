globalThis.instantiateArgon2WasmInstance = async function () {
    await argon2.loadModule();
};

let randomStrings = [
    'iqqdAsbeeb', 'B3tVB5h2AI',
    'Z7ihQxGE93', 'ou2jsFcbur',
    '0pjTjkrnsM', 'B8VRY9tCVY',
    '6Kg3AmWnVc', '3jcEFqDAPT',
    'JeehIKnWiO', '2mSSPZacYX',
    'CGRhFvbj3M', 'aQPgDC7e5j',
    'bSUE8duQ4u', 'Xp3FEiAEXi',
    'tusLv0aHkH', '4GUC7Hx4yk',
    'yJIS8ioQLH', 'OmS9931mEz',
    'ekSwTfKgvC', '3kj2W1fy9F',
    '1oXMME4xPV', 'UDDI3kCDqy',
    'xglo5DvJm3', '2SWNQygbHm',
    'Msfw59Go9X', 'X9vvXuoDM0',
    'XUvEPoHvXC', 'pffHsNtWBg',
    'EQPLyqD9U5', 'yBpDjiKHHj',
    '2gStCWSzhD', 'k74wcxwve7',
    'TU2BIQTMqE', 'bVaCWRL4l3',
    'BMTlPabHpu', 'KXv8hmWApM',
    'lpvttDdRli', 'M4WB62T2qy',
    'FbSoElUBC0', 'QpsrcOC4C1'
];

async function hashAndVerify(params) {
    let hash = await argon2.hash(params);
    await argon2.verify({
        pass: params.pass,
        encoded: hash.encoded,
        type: hash.type,
        ad: params.ad,
        secret: params.secret
    }).catch(e => { throw new Error(e.message) });
}

globalThis.testArgon2HashAndVerify = async function () {
    for (let i = 0; i < randomStrings.length; i += 2) {
        await hashAndVerify({
            pass: randomStrings[i],
            salt: randomStrings[i + 1],
        });
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
