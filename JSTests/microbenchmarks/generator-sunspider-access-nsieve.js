// The Great Computer Language Shootout
// http://shootout.alioth.debian.org/
//
// modified by Isaac Gouy

function *prime(m)
{
   let isPrime = Array(m+1);

   for (let i=2; i<=m; i++) { isPrime[i] = true; }

   for (let i=2; i<=m; i++){
      if (isPrime[i]) {
         for (let k=i+i; k<=m; k+=i) isPrime[k] = false;
         yield i;
      }
   }
}

function sieve() {
    let sum = 0;
    for (let i = 1; i <= 3; i++ ) {
        let m = (1<<i)*10000;
        let count = 0;
        for (let primeNumber of prime(m)) {
            count++;
        }
        sum += count;
    }
    return sum;
}

let result = sieve();

let expected = 14302;
if (result != expected)
    throw "ERROR: bad result: expected " + expected + " but got " + result;
