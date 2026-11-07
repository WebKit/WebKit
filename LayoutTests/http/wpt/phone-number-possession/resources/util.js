// Mirrors the values in Source/WebKit/UIProcess/PhoneNumberPossession/Cocoa/MockPhoneNumberPossessionManager.swift.
const mockResponse = {
    carrierDomain: "carrier.example",
    carrierConfigurationId: 1,
    decryptionKey: { value: 0x04, length: 32 },
    msisdn: { value: 0x07, length: 16 },
    portingZKP: { value: 0x09, length: 32 },
};

const testChallenge = new Uint8Array([0x92, 0xcb, 0xe9, 0xd0, 0xf0, 0xfb, 0x18, 0x30, 0xf6, 0x72, 0x55, 0xed, 0x8a, 0x38, 0x9d, 0x40]);

function setMockConfiguration(configuration = { })
{
    if (window.internals)
        internals.setMockPhoneNumberPossessionConfiguration(configuration);
}

function makeOptions(overrides = { })
{
    return {
        phoneNumberPossessionIdentifier: {
            challenge: testChallenge,
            desiredResponseType: "possession-identifier",
            ...overrides,
        },
    };
}

async function getWithActivation(options)
{
    await test_driver.bless("request phone number possession");
    return navigator.credentials.get(options);
}

function base64URLEncode(bytes)
{
    return btoa(String.fromCharCode(...bytes)).replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}

function decodeJSON(buffer)
{
    return JSON.parse(new TextDecoder().decode(buffer));
}

function assertBufferFilled(buffer, { value, length }, description)
{
    assert_true(buffer instanceof ArrayBuffer, `${description} is an ArrayBuffer`);
    assert_equals(buffer.byteLength, length, `${description} length`);
    assert_true(new Uint8Array(buffer).every(byte => byte === value), `${description} contents`);
}

function promiseRejects(test, expected, promise, message)
{
    return promise.then(test.unreached_func(`Should have rejected: ${message}`), error => {
        if (expected === TypeError)
            assert_true(error instanceof TypeError, `Expected a TypeError, got ${error}`);
        else
            assert_throws_dom(expected, () => { throw error; });
        assert_equals(error.message, message);
    });
}
