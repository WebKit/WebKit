//@ requireOptions("--seedOfVMRandomForFuzzer=2539143341", "--useWideningNumberPredictionFuzzerAgent=1", "--jitPolicyScale=0")

function foo(o) {
  let result = 0;
  for (let i = 0; i < 52; ++i) {
    let value = i | 0;
    switch (value) {
      case 0:
        result += o?.a;
        break;
      case 1:
        result += o?.b;
        break;
      case 2:
        result += o.c;
        break;
      case 3:
        result += o.d;
        break;
      case 4:
        result += o.e;
        break;
      case 5:
        result += o.f;
        break;
      case 6:
        result += o.g;
        break;
      case 7:
        result += o.h;
        break;
      case 8:
        result += o.i;
        break;
      case 9:
        result += o.j;
        break;
      case 10:
        result += o.k;
        break;
      case 11:
        result += o.a;
        break;
      case 12:
        result += o.b;
        break;
      case 13:
        result += o.c;
        break;
      case 14:
        result += o.d;
        break;
      case 15:
        result += o.e;
        break;
      case 16:
        result += o.f;
        break;
      case 17:
        result += o.g;
        break;
      case 18:
        result += o.h;
        break;
      case 19:
        result += o.i;
        break;
      case 20:
        result += o.j;
        break;
      case 21:
        result += o.k;
        break;
      case 22:
        result += o.a;
        break;
      case 23:
        result += o.a;
        break;
      case 24:
        result += o.b;
        break;
      case 25:
        result += o.c;
        break;
      case 26:
        result += o.d;
        break;
      case 27:
        result += o.e;
        break;
      case 28:
        result += o.f;
        break;
      case 29:
        result += o.g;
        break;
      case 30:
        result += o.h;
        break;
      case 31:
        result += o.i;
        break;
      case 32:
        result += o.j;
        break;
      case 33:
        result += o.k;
        break;
      case 34:
        result += o.l;
        break;
      case 36:
        result += o.a;
        break;
      case 37:
        result += o.b;
        break;
      case 38:
        result += o.c;
        break;
      case 39:
        result += o.d;
        break;
      case 40:
        result += o.e;
        break;
      case 41:
        result += o.f;
        break;
      case 42:
        result += o.g;
        break;
      case 43:
        result += o.h;
        break;
      case 44:
        result += o.i;
        break;
      case 45:
        result += o.j;
        break;
      case 46:
        result += o.k;
        break;
      case 47:
        result += o.a;
        break;
      case 48:
        result += o.b;
        break;
      case 49:
        result += o.c;
        break;
      case 50:
        result += o.d;
        break;
      case 51:
        result += o.e;
        break;
    }
  }
  return result;
}

let o = {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, i: 0, j: 0, k: 0, l: 0};
for (let i = 0; i < 100000; ++i) {
  foo(o);
}
