let fastArr   = [1.1, 2.2, 3.3];
fastArr.foo   = 1;
let innerArr  = [100.0, 200.0];
let throwMode = false;
function symIter() { if (throwMode) throw 42; return innerArr[Symbol.iterator](); }
noInline(symIter);
let genericArr = [5.5, 6.6, 7.7];
genericArr[Symbol.iterator] = symIter;
function opt(it){ let r=0; for (let v of it) r+=v; return r; }
noInline(opt);

opt(fastArr);
opt(genericArr);
opt(fastArr);
opt(genericArr);

throwMode = true;
for (let i=0;i<10000;i++){ opt(fastArr); try{ opt(genericArr); }catch(e){} }

throwMode = false;
let r = opt(genericArr);
if (Math.abs(r - (100.0+200.0)) >= 1e-9)
    throw new Error("CheckIsConstant incorrectly elided");
