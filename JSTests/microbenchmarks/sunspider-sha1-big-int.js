/*
 * A JavaScript implementation of the Secure Hash Algorithm, SHA-1, as defined
 * in FIPS PUB 180-1
 * Version 2.1a Copyright Paul Johnston 2000 - 2002.
 * Other contributors: Greg Holt, Andrew Kepert, Ydnar, Lostinet
 * Distributed under the BSD License
 * See http://pajhome.org.uk/crypt/md5 for details.
 */

/*
 * Configurable variables. You may need to tweak these to be compatible with
 * the server-side, but the defaults work in most cases.
 */
var hexcase = 0n;  /* hex output format. 0 - lowercase; 1 - uppercase        */
var b64pad  = ""; /* base-64 pad character. "=" for strict RFC compliance   */
var chrsz   = 8;  /* bits per input character. 8 - ASCII; 16 - Unicode      */

/*
 * These are the functions you'll usually want to call
 * They take string arguments and return either hex or base-64 encoded strings
 */
function hex_sha1(s){return binb2hex(core_sha1(str2binb(s),s.length * chrsz));}
function b64_sha1(s){return binb2b64(core_sha1(str2binb(s),s.length * chrsz));}
function str_sha1(s){return binb2str(core_sha1(str2binb(s),s.length * chrsz));}
function hex_hmac_sha1(key, data){ return binb2hex(core_hmac_sha1(key, data));}
function b64_hmac_sha1(key, data){ return binb2b64(core_hmac_sha1(key, data));}
function str_hmac_sha1(key, data){ return binb2str(core_hmac_sha1(key, data));}

function bigIntToInt32(b) {
    return parseInt(b.toString(), 10);
}

/*
 * Perform a simple self-test to see if the VM is working
 */
function sha1_vm_test()
{
    return hex_sha1("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d";
}

/*
 * Calculate the SHA-1 of an array of big-endian words, and a bit length
 */
//x == str
//len == i32
function core_sha1(x, len)
{
    /* append padding */

    if (x[len >> 5] === undefined)
        x[len >> 5] = 0n;
    if (x[((len + 64 >> 9) << 4) + 15] === undefined)
        x[((len + 64 >> 9) << 4) + 15] = 0n;

    x[len >> 5] |= 0x80n << (24n - BigInt(len) % 32n);
    x[((len + 64 >> 9) << 4) + 15] = BigInt(len);

    var w = Array(80);
    for (let i = 0; i < w.length; ++i) {
        w[i] = 0n;
    }
    var a = 1732584193n;
    var b = -271733879n;
    var c = -1732584194n;
    var d =  271733878n;
    var e = -1009589776n;

    for(var i = 0; i < x.length; i += 16)
    {
        var olda = a;
        var oldb = b;
        var oldc = c;
        var oldd = d;
        var olde = e;

        for(var j = 0; j < 80; j++)
        {
            if (j < 16) w[j] = x[i + j] || 0n;
            else w[j] = rol((w[j-3] || 0n) ^ (w[j-8] || 0n) ^ (w[j-14] || 0n) ^ (w[j-16] || 0n), 1);
            var t = safe_add(safe_add(rol(a, 5), sha1_ft(j, b, c, d)),
                safe_add(safe_add(e, w[j]), sha1_kt(BigInt(j))));
            e = d;
            d = c;

            c = rol(b, 30);
            b = a;
            a = t;
        }

        a = safe_add(a, olda);
        b = safe_add(b, oldb);
        c = safe_add(c, oldc);
        d = safe_add(d, oldd);
        e = safe_add(e, olde);
    }
    return Array(a, b, c, d, e);

}

/*
 * Perform the appropriate triplet combination function for the current
 * iteration
 */
function sha1_ft(t, b, c, d)
{
    if(t < 20) return (b & c) | ((~b) & d);
    if(t < 40) return b ^ c ^ d;
    if(t < 60) return (b & c) | (b & d) | (c & d);
    return b ^ c ^ d;
}

/*
 * Determine the appropriate additive constant for the current iteration
 */
function sha1_kt(t)
{
    return (t < 20n) ?  1518500249n : (t < 40n) ?  1859775393n :
        (t < 60n) ? -1894007588n : -899497514n;
}

/*
 * Calculate the HMAC-SHA1 of a key and some data
 */
function core_hmac_sha1(key, data)
{
    throw new Error("Bad")
    var bkey = str2binb(key);
    if(bkey.length > 16) bkey = core_sha1(bkey, key.length * chrsz);

    var ipad = Array(16), opad = Array(16);
    for(var i = 0; i < 16; i++)
    {
        ipad[i] = bkey[i] ^ 0x36363636n;
        opad[i] = bkey[i] ^ 0x5C5C5C5Cn;
    }

    var hash = core_sha1(ipad.concat(str2binb(data)), 512 + data.length * chrsz);
    return core_sha1(opad.concat(hash), 512 + 160);
}

/*
 * Add integers, wrapping at 2^32. This uses 16-bit operations internally
 * to work around bugs in some JS interpreters.
 */
// bigints
function safe_add(x, y)
{
    var lsw = (x & 0xFFFFn) + (y & 0xFFFFn);
    var msw = ((x >> 16n) & 0xFFFFn) + ((y >> 16n) & 0xFFFFn) + ((lsw >> 16n) & 0xFFFFn);
    let r = (msw << 16n) | (lsw & 0xFFFFn);
    return r;
}

/*
 * Bitwise rotate a 32-bit number to the left.
 */
// bigints arguments
function rol(num, cnt)
{
    let rhs = (num >> (32n - BigInt(cnt)));
    let mask = BigInt((1 << cnt) - 1);
    rhs = rhs & mask;
    let result = ((num << BigInt(cnt)) & 0xFFFFFFFFn) | rhs;
    return result;
}

/*
 * Convert an 8-bit or 16-bit string to an array of big-endian words
 * In 8-bit function, characters >255 have their hi-byte silently ignored.
 */
function str2binb(str)
{
    var bin = Array();
    var mask = ((1 << chrsz) - 1);
    for (var i = 0; i < str.length * chrsz; i += chrsz) {
        if (bin[i>>5] === undefined)
            bin[i>>5] = 0n;
        bin[i>>5] |= BigInt((str.charCodeAt(i / chrsz) & mask) << (32 - chrsz - i%32));
    }
    return bin;
}

/*
 * Convert an array of big-endian words to a string
 */

/*
 * Convert an array of big-endian words to a hex string.
 */
function binb2hex(binarray)
{
    var hex_tab = hexcase ? "0123456789ABCDEF" : "0123456789abcdef";
    var str = "";
    for(var i = 0; i < binarray.length * 4; i++)
    {
        str += hex_tab.charAt(bigIntToInt32((binarray[i>>2] >> BigInt((3 - i%4)*8+4)) & 0xFn)) +
            hex_tab.charAt(bigIntToInt32((binarray[i>>2] >> BigInt((3 - i%4)*8)) & 0xFn));
    }
    return str;
}

/*
 * Convert an array of big-endian words to a base-64 string
 */
function binb2b64(binarray)
{
    var tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    var str = "";
    for(var i = 0; i < binarray.length * 4; i += 3)
    {
        var triplet = (((binarray[i   >> 2] >> 8 * (3 -  i   %4)) & 0xFF) << 16)
            | (((binarray[i+1 >> 2] >> 8 * (3 - (i+1)%4)) & 0xFF) << 8 )
            |  ((binarray[i+2 >> 2] >> 8 * (3 - (i+2)%4)) & 0xFF);
        for(var j = 0; j < 4; j++)
        {
            if(i * 8 + j * 6 > binarray.length * 32) str += b64pad;
            else str += tab.charAt((triplet >> 6*(3-j)) & 0x3F);
        }
    }
    return str;
}


function run() {
    var plainText =
"Two households, both alike in dignity,\n\
In fair Verona, where we lay our scene,\n\
From ancient grudge break to new mutiny,\n\
Where civil blood makes civil hands unclean.\n\
From forth the fatal loins of these two foes\n\
A pair of star-cross'd lovers take their life;\n\
Whole misadventured piteous overthrows\n\
Do with their death bury their parents' strife.\n\
The fearful passage of their death-mark'd love,\n\
And the continuance of their parents' rage,\n\
Which, but their children's end, nought could remove,\n\
Is now the two hours' traffic of our stage;\n\
The which if you with patient ears attend,\n\
What here shall miss, our toil shall strive to mend.";

    for (var i = 0; i <4; i++) {
        plainText += plainText;
    }

    var sha1Output = hex_sha1(plainText);

    var expected = "2524d264def74cce2498bf112bedf00e6c0b796d";
    if (sha1Output != expected)
        throw "ERROR: bad result: expected " + expected + " but got " + sha1Output;
}


let start = Date.now();
for (let i = 0; i < 5; ++i)
    run();
// print("benchmark time:", Date.now() - start, "ms");
