const testCredentialIdBase64 = "SMSXHngF7hEOsElA73C3RY+8bR4=";
const testES256PrivateKeyBase64 =
    "BDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749VBJPgqUIwfhWHJ91nb7U" +
    "PH76c0+WFOzZKslPyyFse4goGIW2R7k9VHLPEZl5nfnBgEVFh5zev+/xpHQIvuq6" +
    "RQ==";
const testRpId = "localhost";
const testUserhandleBase64 = "AAECAwQFBgcICQ==";
const testUserEntityBundleBase64 = "omJpZEoAAQIDBAUGBwgJZG5hbWVwQUFFQ0F3UUZCZ2NJQ1E9PQ==";
const testAttestationCertificateBase64 =
    "MIIB6jCCAZCgAwIBAgIGAWHAxcjvMAoGCCqGSM49BAMCMFMxJzAlBgNVBAMMHkJh" +
    "c2ljIEF0dGVzdGF0aW9uIFVzZXIgU3ViIENBMTETMBEGA1UECgwKQXBwbGUgSW5j" +
    "LjETMBEGA1UECAwKQ2FsaWZvcm5pYTAeFw0xODAyMjMwMzM3MjJaFw0xODAyMjQw" +
    "MzQ3MjJaMGoxIjAgBgNVBAMMGTAwMDA4MDEwLTAwMEE0OUEyMzBBMDIxM0ExGjAY" +
    "BgNVBAsMEUJBQSBDZXJ0aWZpY2F0aW9uMRMwEQYDVQQKDApBcHBsZSBJbmMuMRMw" +
    "EQYDVQQIDApDYWxpZm9ybmlhMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEvCje" +
    "Pzr6Sg76XMoHuGabPaG6zjpLFL8Zd8/74Hh5PcL2Zq+o+f7ENXX+7nEXXYt0S8Ux" +
    "5TIRw4hgbfxXQbWLEqM5MDcwDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMCBPAw" +
    "FwYJKoZIhvdjZAgCBAowCKEGBAR0ZXN0MAoGCCqGSM49BAMCA0gAMEUCIAlK8A8I" +
    "k43TbvKuYGHZs1DTgpTwmKTBvIUw5bwgZuYnAiEAtuJjDLKbGNJAJFMi5deEBqno" +
    "pBTCqbfbDJccfyQpjnY=";
const testAttestationIssuingCACertificateBase64 =
    "MIICIzCCAaigAwIBAgIIeNjhG9tnDGgwCgYIKoZIzj0EAwIwUzEnMCUGA1UEAwwe" +
    "QmFzaWMgQXR0ZXN0YXRpb24gVXNlciBSb290IENBMRMwEQYDVQQKDApBcHBsZSBJ" +
    "bmMuMRMwEQYDVQQIDApDYWxpZm9ybmlhMB4XDTE3MDQyMDAwNDIwMFoXDTMyMDMy" +
    "MjAwMDAwMFowUzEnMCUGA1UEAwweQmFzaWMgQXR0ZXN0YXRpb24gVXNlciBTdWIg" +
    "Q0ExMRMwEQYDVQQKDApBcHBsZSBJbmMuMRMwEQYDVQQIDApDYWxpZm9ybmlhMFkw" +
    "EwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEoSZ/1t9eBAEVp5a8PrXacmbGb8zNC1X3" +
    "StLI9YO6Y0CL7blHmSGmjGWTwD4Q+i0J2BY3+bPHTGRyA9jGB3MSbaNmMGQwEgYD" +
    "VR0TAQH/BAgwBgEB/wIBADAfBgNVHSMEGDAWgBSD5aMhnrB0w/lhkP2XTiMQdqSj" +
    "8jAdBgNVHQ4EFgQU5mWf1DYLTXUdQ9xmOH/uqeNSD80wDgYDVR0PAQH/BAQDAgEG" +
    "MAoGCCqGSM49BAMCA2kAMGYCMQC3M360LLtJS60Z9q3vVjJxMgMcFQ1roGTUcKqv" +
    "W+4hJ4CeJjySXTgq6IEHn/yWab4CMQCm5NnK6SOSK+AqWum9lL87W3E6AA1f2TvJ" +
    "/hgok/34jr93nhS87tOQNdxDS8zyiqw=";
const testDummyMessagePayloadBase64 =
    "/////wYAEQABAgMEBQYHAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" +
    "AAAAAAAAAAAAAAAAAAAAAAAAAAEQADoAAQIDBAUGBwECAwQAAAAAAAAAAAAAAAAA" +
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAgMEAAAAAAAAAAAAAAAA" +
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const testCreationMessageBase64 =
    "AKMBZnBhY2tlZAJYxEbMf7lnnVWy25CS4cjZ5eHQK3WA8LSBLHcJYuHkj1rYQQAA" +
    "AE74oBHzjApNFYAGFxEfntx9AEAoCK3O6P5OyXN6V/f+9nAga0NA2Cgp4V3mgSJ5" +
    "jOHLMDrmxp/S0rbD+aihru1C0aAN3BkiM6GNy5nSlDVqOgTgpQECAyYgASFYIEFb" +
    "he3RkNud6sgyraBGjlh1pzTlCZehQlL/b18HZ6WGIlggJgfUd/en9p5AIqMQbUni" +
    "nEeXdFLkvW0/zV5BpEjjNxADo2NhbGcmY3NpZ1hHMEUCIQDKg+ZBmEBtf0lWq4Re" +
    "dH4/i/LOYqOR4uR2NAj2zQmw9QIgbTXb4hvFbj4T27bv/rGrc+y+0puoYOBkBk9P" +
    "mCewWlNjeDVjgVkCwjCCAr4wggGmoAMCAQICBHSG/cIwDQYJKoZIhvcNAQELBQAw" +
    "LjEsMCoGA1UEAxMjWXViaWNvIFUyRiBSb290IENBIFNlcmlhbCA0NTcyMDA2MzEw" +
    "IBcNMTQwODAxMDAwMDAwWhgPMjA1MDA5MDQwMDAwMDBaMG8xCzAJBgNVBAYTAlNF" +
    "MRIwEAYDVQQKDAlZdWJpY28gQUIxIjAgBgNVBAsMGUF1dGhlbnRpY2F0b3IgQXR0" +
    "ZXN0YXRpb24xKDAmBgNVBAMMH1l1YmljbyBVMkYgRUUgU2VyaWFsIDE5NTUwMDM4" +
    "NDIwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASVXfOt9yR9MXXv/ZzE8xpOh466" +
    "4YEJVmFQ+ziLLl9lJ79XQJqlgaUNCsUvGERcChNUihNTyKTlmnBOUjvATevto2ww" +
    "ajAiBgkrBgEEAYLECgIEFTEuMy42LjEuNC4xLjQxNDgyLjEuMTATBgsrBgEEAYLl" +
    "HAIBAQQEAwIFIDAhBgsrBgEEAYLlHAEBBAQSBBD4oBHzjApNFYAGFxEfntx9MAwG" +
    "A1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggEBADFcSIDmmlJ+OGaJvWn9Cqhv" +
    "SeueToVFQVVvqtALOgCKHdwB+Wx29mg2GpHiMsgQp5xjB0ybbnpG6x212FxESJ+G" +
    "inZD0ipchi7APwPlhIvjgH16zVX44a4e4hOsc6tLIOP71SaMsHuHgCcdH0vg5d2s" +
    "c006WJe9TXO6fzV+ogjJnYpNKQLmCXoAXE3JBNwKGBIOCvfQDPyWmiiG5bGxYfPt" +
    "y8Z3pnjX+1MDnM2hhr40ulMxlSNDnX/ZSnDyMGIbk8TOQmjTF02UO8auP8k3wt5D" +
    "1rROIRU9+FCSX5WQYi68RuDrGMZB8P5+byoJqbKQdxn2LmE1oZAyohPAmLcoPO4=";
const testHidCredentialIdBase64 =
    "KAitzuj-Tslzelf3_vZwIGtDQNgoKeFd5oEieYzhyzA65saf0tK2w_mooa7tQtGg" +
    "DdwZIjOhjcuZ0pQ1ajoE4A";
const testAssertionMessageBase64 =
    "AKMBomJpZFhAKAitzuj+Tslzelf3/vZwIGtDQNgoKeFd5oEieYzhyzA65saf0tK2" +
    "w/mooa7tQtGgDdwZIjOhjcuZ0pQ1ajoE4GR0eXBlanB1YmxpYy1rZXkCWCVGzH+5" +
    "Z51VstuQkuHI2eXh0Ct1gPC0gSx3CWLh5I9a2AEAAABQA1hHMEUCIQCSFTuuBWgB" +
    "4/F0VB7DlUVM09IHPmxe1MzHUwRoCRZbCAIgGKov6xoAx2MEf6/6qNs8OutzhP2C" +
    "QoJ1L7Fe64G9uBc=";
const testAssertionMessageLongBase64 =
    "AKUBomJpZFhAKAitzuj+Tslzelf3/vZwIGtDQNgoKeFd5oEieYzhyzA65saf0tK2" +
    "w/mooa7tQtGgDdwZIjOhjcuZ0pQ1ajoE4GR0eXBlanB1YmxpYy1rZXkCWCVGzH+5" +
    "Z51VstuQkuHI2eXh0Ct1gPC0gSx3CWLh5I9a2AEAAABQA1hHMEUCIQCSFTuuBWgB" +
    "4/F0VB7DlUVM09IHPmxe1MzHUwRoCRZbCAIgGKov6xoAx2MEf6/6qNs8OutzhP2C" +
    "QoJ1L7Fe64G9uBcEpGJpZFggMIIBkzCCATigAwIBAjCCAZMwggE4oAMCAQIwggGT" +
    "MIJkaWNvbngoaHR0cHM6Ly9waWNzLmFjbWUuY29tLzAwL3AvYUJqampwcVBiLnBu" +
    "Z2RuYW1ldmpvaG5wc21pdGhAZXhhbXBsZS5jb21rZGlzcGxheU5hbWVtSm9obiBQ" +
    "LiBTbWl0aAUC";
const testU2fApduNoErrorOnlyResponseBase64 = "kAA=";
const testU2fApduInsNotSupportedOnlyResponseBase64 = "bQA=";
const testU2fApduWrongDataOnlyResponseBase64 = "aoA=";
const testU2fApduConditionsNotSatisfiedOnlyResponseBase64 = "aYU=";
const testU2fRegisterResponse =
    "BQTodiWJbuTkbcAydm6Ah5YvNt+d/otWfzdjAVsZkKYOFCfeYS1mQYvaGVBYHrxc" +
    "jB2tcQyxTCL4yXBF9GEvsgyRQD69ib937FCXVe6cJjXvqqx7K5xc7xc2w3F9pIU0" +
    "yMa2VNf/lF9QtcxOeAVb3TlrZPeNosX5YgDM1BXNCP5CADgwggJKMIIBMqADAgEC" +
    "AgQEbIgiMA0GCSqGSIb3DQEBCwUAMC4xLDAqBgNVBAMTI1l1YmljbyBVMkYgUm9v" +
    "dCBDQSBTZXJpYWwgNDU3MjAwNjMxMCAXDTE0MDgwMTAwMDAwMFoYDzIwNTAwOTA0" +
    "MDAwMDAwWjAsMSowKAYDVQQDDCFZdWJpY28gVTJGIEVFIFNlcmlhbCAyNDkxODIz" +
    "MjQ3NzAwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQ8yrksy5cofujmOUN+IfzW" +
    "tvFlstWj89sTHTHBa3QrtHbY0emQgOtUbJu99VbmIQ/UJ4WJnnjMWJ6+MQ9s25/0" +
    "ozswOTAiBgkrBgEEAYLECgIEFTEuMy42LjEuNC4xLjQxNDgyLjEuMjATBgsrBgEE" +
    "AYLlHAIBAQQEAwIEMDANBgkqhkiG9w0BAQsFAAOCAQEAn5sFIki8TPQsxZkfyqus" +
    "m2Ubvlvc3I7wrSwcH/s20YcV1C54skkiT5LH5uegXEnw5+TIgb8ulPReSiGDPXRW" +
    "hR0PbBRaKVQMh08wksk0tD0iK4liwPQQzvHbdYkq8Ra0Spb101reo4IvxxRvYAQ4" +
    "W8tptlyZ5+tpGXhnA8DYzUHo91zKRKqKtyWtjnmf86hpam8bJlbmMbHkAYPAj9pT" +
    "+kqPhaBWk5RK4XmhM50ALRXKvYEAkOxyLvXe+ZZaNx1BXWJLaKJwfK2XvN0Xha+X" +
    "4ljzPfVqAxqgNW2OjV68rcdOBxY2xrEQrOXMm5Df6srmQP8bsPH+XbTv96lfBgcz" +
    "9TBFAiAyR3nGjzOAKIoRl7YJX3puubGxwSf2auEqmf6FMuwjuQIhAOOVFqxNYe5k" +
    "BE1QtBWmpNTYS6bYlctat6GqfQgd40H6kAA=";
const testU2fCredentialIdBase64 =
    "Pr2Jv3fsUJdV7pwmNe-qrHsrnFzvFzbDcX2khTTIxrZU1_-UX1C1zE54BVvdOWtk" +
    "942ixfliAMzUFc0I_kIAOA";
const testU2fSignResponse =
    "AQAAADswRAIge94KUqwfTIsn4AOjcM1mpMcRjdItVEeDX0W5nGhCP/cCIDxRe0eH" +
    "f4V4LeEAhqeD0effTjY553H19q+jWq1Tc4WOkAA=";
const testCtapErrCredentialExcludedOnlyResponseBase64 = "GQ==";
const testCtapErrInvalidCredentialResponseBase64 = "Ig==";
const testCtapErrNotAllowedResponseBase64 = "MA==";
const testNfcU2fVersionBase64 = "VTJGX1YykAA=";
const testNfcCtapVersionBase64 = "RklET18yXzCQAA==";
const testGetInfoResponseApduBase64 =
    "AKYBgmZVMkZfVjJoRklET18yXzACgWtobWFjLXNlY3JldANQbUS6m/bsLkm5MAyP" +
    "6SDLcwSkYnJr9WJ1cPVkcGxhdPRpY2xpZW50UGlu9AUZBLAGgQGQAA==";
const testCreationMessageApduBase64 =
    "AKMBZnBhY2tlZAJYxEbMf7lnnVWy25CS4cjZ5eHQK3WA8LSBLHcJYuHkj1rYQQAA" +
    "AE74oBHzjApNFYAGFxEfntx9AEAoCK3O6P5OyXN6V/f+9nAga0NA2Cgp4V3mgSJ5" +
    "jOHLMDrmxp/S0rbD+aihru1C0aAN3BkiM6GNy5nSlDVqOgTgpQECAyYgASFYIEFb" +
    "he3RkNud6sgyraBGjlh1pzTlCZehQlL/b18HZ6WGIlggJgfUd/en9p5AIqMQbUni" +
    "nEeXdFLkvW0/zV5BpEjjNxADo2NhbGcmY3NpZ1hHMEUCIQDKg+ZBmEBtf0lWq4Re" +
    "dH4/i/LOYqOR4uR2NAj2zQmw9QIgbTXb4hvFbj4T27bv/rGrc+y+0puoYOBkBk9P" +
    "mCewWlNjeDVjgVkCwjCCAr4wggGmoAMCAQICBHSG/cIwDQYJKoZIhvcNAQELBQAw" +
    "LjEsMCoGA1UEAxMjWXViaWNvIFUyRiBSb290IENBIFNlcmlhbCA0NTcyMDA2MzEw" +
    "IBcNMTQwODAxMDAwMDAwWhgPMjA1MDA5MDQwMDAwMDBaMG8xCzAJBgNVBAYTAlNF" +
    "MRIwEAYDVQQKDAlZdWJpY28gQUIxIjAgBgNVBAsMGUF1dGhlbnRpY2F0b3IgQXR0" +
    "ZXN0YXRpb24xKDAmBgNVBAMMH1l1YmljbyBVMkYgRUUgU2VyaWFsIDE5NTUwMDM4" +
    "NDIwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASVXfOt9yR9MXXv/ZzE8xpOh466" +
    "4YEJVmFQ+ziLLl9lJ79XQJqlgaUNCsUvGERcChNUihNTyKTlmnBOUjvATevto2ww" +
    "ajAiBgkrBgEEAYLECgIEFTEuMy42LjEuNC4xLjQxNDgyLjEuMTATBgsrBgEEAYLl" +
    "HAIBAQQEAwIFIDAhBgsrBgEEAYLlHAEBBAQSBBD4oBHzjApNFYAGFxEfntx9MAwG" +
    "A1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggEBADFcSIDmmlJ+OGaJvWn9Cqhv" +
    "SeueToVFQVVvqtALOgCKHdwB+Wx29mg2GpHiMsgQp5xjB0ybbnpG6x212FxESJ+G" +
    "inZD0ipchi7APwPlhIvjgH16zVX44a4e4hOsc6tLIOP71SaMsHuHgCcdH0vg5d2s" +
    "c006WJe9TXO6fzV+ogjJnYpNKQLmCXoAXE3JBNwKGBIOCvfQDPyWmiiG5bGxYfPt" +
    "y8Z3pnjX+1MDnM2hhr40ulMxlSNDnX/ZSnDyMGIbk8TOQmjTF02UO8auP8k3wt5D" +
    "1rROIRU9+FCSX5WQYi68RuDrGMZB8P5+byoJqbKQdxn2LmE1oZAyohPAmLcoPO6Q" +
    "AA==";
const testAssertionMessageApduBase64 =
    "AKMBomJpZFhAKAitzuj+Tslzelf3/vZwIGtDQNgoKeFd5oEieYzhyzA65saf0tK2" +
    "w/mooa7tQtGgDdwZIjOhjcuZ0pQ1ajoE4GR0eXBlanB1YmxpYy1rZXkCWCVGzH+5" +
    "Z51VstuQkuHI2eXh0Ct1gPC0gSx3CWLh5I9a2AEAAABQA1hHMEUCIQCSFTuuBWgB" +
    "4/F0VB7DlUVM09IHPmxe1MzHUwRoCRZbCAIgGKov6xoAx2MEf6/6qNs8OutzhP2C" +
    "QoJ1L7Fe64G9uBeQAA==";
const testCcidNoUidBase64 = "aIE=";
const testCcidValidUidBase64 = "CH+d1ZAA";

const RESOURCES_DIR = "/WebKit/webauthn/resources/";

function asciiToUint8Array(str)
{
    var chars = [];
    for (var i = 0; i < str.length; ++i)
        chars.push(str.charCodeAt(i));
    return new Uint8Array(chars);
}

// Builds a hex string representation for an array-like input.
// "bytes" can be an Array of bytes, an ArrayBuffer, or any TypedArray.
// The output looks like this:
//    ab034c99
function bytesToHexString(bytes)
{
    if (!bytes)
        return null;

    bytes = new Uint8Array(bytes);
    var hexBytes = [];

    for (var i = 0; i < bytes.length; ++i) {
        var byteString = bytes[i].toString(16);
        if (byteString.length < 2)
            byteString = "0" + byteString;
        hexBytes.push(byteString);
    }

    return hexBytes.join("");
}

function bytesToASCIIString(bytes)
{
    return String.fromCharCode.apply(null, new Uint8Array(bytes));
}

var Base64URL = {
    stringify: function (a) {
        var base64string = btoa(String.fromCharCode.apply(0, a));
        return base64string.replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
    },
    parse: function (s) {
        s = s.replace(/-/g, "+").replace(/_/g, "/").replace(/\s/g, '');
        return new Uint8Array(Array.prototype.map.call(atob(s), function (c) { return c.charCodeAt(0) }));
    }
};

function hexStringToUint8Array(hexString)
{
    if (hexString.length % 2 != 0)
        throw "Invalid hexString";
    var arrayBuffer = new Uint8Array(hexString.length / 2);

    for (var i = 0; i < hexString.length; i += 2) {
        var byteValue = parseInt(hexString.substr(i, 2), 16);
        if (byteValue == NaN)
            throw "Invalid hexString";
        arrayBuffer[i/2] = byteValue;
    }

    return arrayBuffer;
}

function decodeAuthData(authDataUint8Array)
{
    let authDataObject = { };
    let pos = 0;
    let size = 0;

    // RP ID Hash
    size = 32;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.rpIdHash = authDataUint8Array.slice(pos, pos + size);
    pos = pos + size;

    // FLAGS
    size = 1;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.flags = authDataUint8Array.slice(pos, pos + size)[0];
    pos = pos + size;

    // Counter
    size = 4;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.counter = (authDataUint8Array[pos] << 24) + (authDataUint8Array[pos + 1] << 16) + (authDataUint8Array[pos + 2] << 8) + authDataUint8Array[pos + 3];
    pos = pos + size;

    if (pos == authDataUint8Array.byteLength)
        return authDataObject;

    // AAGUID
    size = 16;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.aaguid = authDataUint8Array.slice(pos, pos + size);
    pos = pos + size;

    // L
    size = 2;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.l = (authDataUint8Array[pos] << 8) + authDataUint8Array[pos + 1];
    pos = pos + size;

    // Credential ID
    size = authDataObject.l;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.credentialID = authDataUint8Array.slice(pos, pos + size);
    pos = pos + size;

    // Public Key
    authDataObject.publicKey = CBOR.decode(authDataUint8Array.slice(pos).buffer);
    if (!authDataObject.publicKey)
        return { };

    // Assume no extensions.
    return authDataObject;
}

function concatenateBuffers(buffer1, buffer2)
{
    let tmp = new Uint8Array(buffer1.byteLength + buffer2.byteLength);
    tmp.set(new Uint8Array(buffer1), 0);
    tmp.set(new Uint8Array(buffer2), buffer1.byteLength);
    return tmp.buffer;
}

// Very dirty asn1 decoder. Just works.
function extractRawSignature(asn1signature)
{
    const signature = new Uint8Array(asn1signature);
    let tmp = new Uint8Array(64);

    const rStart =  signature[3] - 32;
    if (rStart >= 0)
        tmp.set(new Uint8Array(signature.slice(4 + rStart, 36 + rStart)), 0);
    else
        tmp.set(new Uint8Array(signature.slice(4, 36 + rStart)), -rStart);

    const sStart =  signature[37 + rStart] - 32;
    if (sStart >= 0)
        tmp.set(new Uint8Array(signature.slice(38 + rStart + sStart)), 32);
    else
        tmp.set(new Uint8Array(signature.slice(38 + rStart)), 32 - sStart);

    return tmp.buffer;
}

function waitForLoad()
{
    return new Promise((resolve) => {
        window.addEventListener('message', (message) => {
            resolve(message);
        });
    });
}

function withCrossOriginIframe(resourceFile, allow = "")
{
    return new Promise((resolve) => {
        waitForLoad().then((message) => {
            resolve(message);
        });
        const frame = document.createElement("iframe");
        frame.allow = allow;
        frame.src = get_host_info().HTTPS_REMOTE_ORIGIN + RESOURCES_DIR + resourceFile;
        document.body.appendChild(frame);
    });
}

function withSameSiteIframe(resourceFile, allow = "")
{
    return new Promise((resolve) => {
        waitForLoad().then((message) => {
            resolve(message);
       });
       const frame = document.createElement("iframe");
       const host = get_host_info();
       frame.allow = allow;
       frame.src = "https://" + host.ORIGINAL_HOST + ":" + host.HTTPS_PORT2 + RESOURCES_DIR + resourceFile;
       document.body.appendChild(frame);
    });
}

function promiseRejects(test, expected, promise, description)
{
    return promise.then(test.unreached_func("Should have rejected: " + description)).catch(function(e) {
        assert_throws_dom(expected, function() { throw e }, description);
        assert_equals(e.message, description);
    });
}

// COSE Key Format: https://www.w3.org/TR/webauthn/#sctn-encoded-credPubKey-examples.
function checkPublicKey(publicKey)
{
    if (publicKey['1'] != 2)
        return false;
    if (publicKey['3'] != -7)
        return false;
    if (publicKey['-1'] != 1)
        return false;
    if (publicKey['-2'].byteLength != 32)
        return false;
    if (publicKey['-3'].byteLength != 32)
        return false;
    return true;
}

function checkCtapMakeCredentialResult(credential, isNoneAttestation = true, transports = null)
{
    // Check response
    assert_array_equals(Base64URL.parse(credential.id), Base64URL.parse(testHidCredentialIdBase64));
    assert_equals(credential.type, 'public-key');
    assert_array_equals(new Uint8Array(credential.rawId), Base64URL.parse(testHidCredentialIdBase64));
    assert_equals(bytesToASCIIString(credential.response.clientDataJSON), '{"type":"webauthn.create","challenge":"MTIzNDU2","origin":"https://localhost:9443"}');
    assert_not_own_property(credential.getClientExtensionResults(), "appid");

    // Check attestation
    const attestationObject = CBOR.decode(credential.response.attestationObject);
    if (isNoneAttestation)
        assert_equals(attestationObject.fmt, "none");
    else
        assert_equals(attestationObject.fmt, "packed");
    // Check authData
    const authData = decodeAuthData(attestationObject.authData);
    assert_equals(bytesToHexString(authData.rpIdHash), "46cc7fb9679d55b2db9092e1c8d9e5e1d02b7580f0b4812c770962e1e48f5ad8");
    assert_equals(authData.flags, 65);
    assert_equals(authData.counter, 78);
    if (isNoneAttestation)
        assert_equals(bytesToHexString(authData.aaguid), "00000000000000000000000000000000");
    else
        assert_equals(bytesToHexString(authData.aaguid), "f8a011f38c0a4d15800617111f9edc7d");
    assert_array_equals(authData.credentialID, Base64URL.parse(testHidCredentialIdBase64));
    // Check packed attestation
    assert_true(checkPublicKey(authData.publicKey));
    if (isNoneAttestation)
        assert_object_equals(attestationObject.attStmt, { });
    else {
        assert_equals(attestationObject.attStmt.alg, -7);
        assert_equals(attestationObject.attStmt.x5c.length, 1);
    }
    if (transports)
        assert_array_equals(transports, credential.response.getTransports());
}

function checkU2fMakeCredentialResult(credential, isNoneAttestation = true)
{
    // Check response
    assert_array_equals(Base64URL.parse(credential.id), Base64URL.parse(testU2fCredentialIdBase64));
    assert_equals(credential.type, 'public-key');
    assert_array_equals(new Uint8Array(credential.rawId), Base64URL.parse(testU2fCredentialIdBase64));
    assert_equals(bytesToASCIIString(credential.response.clientDataJSON), '{"type":"webauthn.create","challenge":"MTIzNDU2","origin":"https://localhost:9443"}');
    assert_not_own_property(credential.getClientExtensionResults(), "appid");

    // Check attestation
    const attestationObject = CBOR.decode(credential.response.attestationObject);
    if (isNoneAttestation)
        assert_equals(attestationObject.fmt, "none");
    else
        assert_equals(attestationObject.fmt, "fido-u2f");
    // Check authData
    const authData = decodeAuthData(attestationObject.authData);
    assert_equals(bytesToHexString(authData.rpIdHash), "49960de5880e8c687434170f6476605b8fe4aeb9a28632c7995cf3ba831d9763");
    assert_equals(authData.flags, 65);
    assert_equals(authData.counter, 0);
    assert_equals(bytesToHexString(authData.aaguid), "00000000000000000000000000000000");
    assert_array_equals(authData.credentialID, Base64URL.parse(testU2fCredentialIdBase64));
    // Check fido-u2f attestation
    assert_true(checkPublicKey(authData.publicKey));
    if (isNoneAttestation)
        assert_object_equals(attestationObject.attStmt, { });
    else
        assert_equals(attestationObject.attStmt.x5c.length, 1);
}

function checkCtapGetAssertionResult(credential, userHandleBase64 = null)
{
    // Check respond
    assert_array_equals(Base64URL.parse(credential.id), Base64URL.parse(testHidCredentialIdBase64));
    assert_equals(credential.type, 'public-key');
    assert_equals(credential.authenticatorAttachment, 'cross-platform')
    assert_array_equals(new Uint8Array(credential.rawId), Base64URL.parse(testHidCredentialIdBase64));
    assert_equals(bytesToASCIIString(credential.response.clientDataJSON), '{"type":"webauthn.get","challenge":"MTIzNDU2","origin":"https://localhost:9443"}');
    if (userHandleBase64 == null)
        assert_equals(credential.response.userHandle, null);
    else
        assert_array_equals(new Uint8Array(credential.response.userHandle), Base64URL.parse(userHandleBase64));
    assert_not_own_property(credential.getClientExtensionResults(), "appid");

    // Check authData
    const authData = decodeAuthData(new Uint8Array(credential.response.authenticatorData));
    assert_equals(bytesToHexString(authData.rpIdHash), "46cc7fb9679d55b2db9092e1c8d9e5e1d02b7580f0b4812c770962e1e48f5ad8");
    assert_equals(authData.flags, 1);
    assert_equals(authData.counter, 80);
}

function checkU2fGetAssertionResult(credential, isAppID = false, appIDHash = "c2671b6eb9233197d5f2b1288a55ba4f0860f96f7199bba32fe6da7c3f0f31e5")
{
    // Check respond
    assert_array_equals(Base64URL.parse(credential.id), Base64URL.parse(testU2fCredentialIdBase64));
    assert_equals(credential.type, 'public-key');
    assert_array_equals(new Uint8Array(credential.rawId), Base64URL.parse(testU2fCredentialIdBase64));
    assert_equals(bytesToASCIIString(credential.response.clientDataJSON), '{"type":"webauthn.get","challenge":"MTIzNDU2","origin":"https://localhost:9443"}');
    assert_equals(credential.response.userHandle, null);
    if (!isAppID)
        assert_not_own_property(credential.getClientExtensionResults(), "appid");
    else
        assert_true(credential.getClientExtensionResults().appid);

    // Check authData
    const authData = decodeAuthData(new Uint8Array(credential.response.authenticatorData));
    if (!isAppID)
        assert_equals(bytesToHexString(authData.rpIdHash), "49960de5880e8c687434170f6476605b8fe4aeb9a28632c7995cf3ba831d9763");
    else
        assert_equals(bytesToHexString(authData.rpIdHash), appIDHash);
    assert_equals(authData.flags, 1);
    assert_equals(authData.counter, 59);
}

function generateUserhandleBase64()
{
    let buffer = new Uint8Array(8);
    crypto.getRandomValues(buffer);
    return btoa(String.fromCharCode.apply(0, buffer));
}

async function generatePrivateKeyBase64()
{
    const algorithmKeyGen = {
        name: "ECDSA",
        namedCurve: "P-256"
    };
    const extractable = true;

    const keyPair = await crypto.subtle.generateKey(algorithmKeyGen, extractable, ["sign", "verify"]);
    const jwkPrivateKey = await crypto.subtle.exportKey("jwk", keyPair.privateKey);

    const x = Base64URL.parse(jwkPrivateKey.x);
    const y = Base64URL.parse(jwkPrivateKey.y);
    const d = Base64URL.parse(jwkPrivateKey.d);

    let buffer = new Uint8Array(x.length + y.length + d.length + 1);
    buffer[0] = 0x04;
    let pos = 1;
    buffer.set(x, pos);
    pos += x.length;
    buffer.set(y, pos);
    pos += y.length;
    buffer.set(d, pos);

    return btoa(String.fromCharCode.apply(0, buffer));
}

async function calculateCredentialID(privateKeyBase64) {
    const privateKey = Base64URL.parse(privateKeyBase64);
    const publicKey = privateKey.slice(0, 65);
    return new Uint8Array(await crypto.subtle.digest("sha-1", publicKey));
}

function base64encode(binary)
{
    const unit8Array = new Uint8Array(binary);
    return btoa(String.fromCharCode.apply(0, unit8Array));
}
