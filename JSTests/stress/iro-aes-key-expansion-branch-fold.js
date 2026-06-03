// The AES constructor and its table precompute are copied verbatim from
// PerformanceTests/JetStream3/SeaMonster/sjlc.js.

var sjcl = { cipher: {}, exception: { invalid: function(a) { this.message = a; } } };
sjcl.cipher.aes=function(a){this.s[0][0][0]||this.O();var b,c,d,e,f=this.s[0][4],g=this.s[1];b=a.length;var h=1;if(4!==b&&6!==b&&8!==b)throw new sjcl.exception.invalid("invalid aes key size");this.b=[d=a.slice(0),e=[]];for(a=b;a<4*b+28;a++){c=d[a-1];if(0===a%b||8===b&&4===a%b)c=f[c>>>24]<<24^f[c>>16&255]<<16^f[c>>8&255]<<8^f[c&255],0===a%b&&(c=c<<8^c>>>24^h<<24,h=h<<1^283*(h>>7));d[a]=d[a-b]^c}for(b=0;a;b++,a--)c=d[b&3?a:a-4],e[b]=4>=a||4>b?c:g[0][f[c>>>24]]^g[1][f[c>>16&255]]^g[2][f[c>>8&255]]^g[3][f[c&
255]]};
sjcl.cipher.aes.prototype={encrypt:function(a){return t(this,a,0)},decrypt:function(a){return t(this,a,1)},s:[[[],[],[],[],[]],[[],[],[],[],[]]],O:function(){var a=this.s[0],b=this.s[1],c=a[4],d=b[4],e,f,g,h=[],k=[],l,n,m,p;for(e=0;0x100>e;e++)k[(h[e]=e<<1^283*(e>>7))^e]=e;for(f=g=0;!c[f];f^=l||1,g=k[g]||1)for(m=g^g<<1^g<<2^g<<3^g<<4,m=m>>8^m&255^99,c[f]=m,d[m]=f,n=h[e=h[l=h[f]]],p=0x1010101*n^0x10001*e^0x101*l^0x1010100*f,n=0x101*h[m]^0x1010100*m,e=0;4>e;e++)a[e][f]=n=n<<24^n>>>8,b[e][m]=p=p<<24^p>>>8;for(e=
0;5>e;e++)a[e]=a[e].slice(0),b[e]=b[e].slice(0)}};

function check(keyLen) {
    const key = [];
    for (let i = 0; i < keyLen; i++)
        key[i] = (i * 0x9e3779b1) | 0;
    const aes = new sjcl.cipher.aes(key);
    // aes.b[1] is the decryption key schedule, built by the second loop which
    // iterates exactly 4*keyLen+28 times.
    const expected = 4 * keyLen + 28;
    if (aes.b[1].length !== expected)
        throw new Error("MISCOMPILE: keyLen=" + keyLen + " produced a decryption "
            + "schedule of " + aes.b[1].length + " words, expected " + expected);
}
noInline(check);

for (let i = 0; i < 100000; ++i) {
    check(4);
    check(6);
    check(8);
}
