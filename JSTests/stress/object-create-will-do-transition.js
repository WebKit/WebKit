var p = 0;
var q=[]
function __f_0(a,b,__v_6,__v_8) {p=p+1;
if(p > 5000) {
    q.push(a);
    q.push(b);
    q.push(__v_6);
    q.push(__v_8);
    return ;
  }
  var a = {};
  var b = {};
  __v_6 = Object.create(a);
  a.method1 = 1;
  b.method1 = 1;
  __v_8 = Object.create(a);
  __f_0(a,b,__v_6,__v_8);
}

optimizeNextInvocation(__f_0);
__f_0();
