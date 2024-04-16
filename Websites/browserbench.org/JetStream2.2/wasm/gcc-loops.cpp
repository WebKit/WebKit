#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <numeric>

/// This test contains some of the loops from the GCC vectrorizer example page [1].
/// Dorit Nuzman who developed the gcc vectorizer said that we can use them in our test suite.
///
/// [1] - http://gcc.gnu.org/projects/tree-ssa/vectorization.html

#define N 1024
#define M 32
#define K 4
#define ALIGNED16 __attribute__((aligned(16)))

unsigned short usa[N];
short sa[N];
short sb[N];
short sc[N];
unsigned int   ua[N];
int   ia[N] ALIGNED16;
int   ib[N] ALIGNED16;
int   ic[N] ALIGNED16;
unsigned int ub[N];
unsigned int uc[N];
float fa[N], fb[N];
float da[N], db[N], dc[N], dd[N];
int dj[N];

struct A {
  int ca[N];
} s;

int a[N*2] ALIGNED16;
int b[N*2] ALIGNED16;
int c[N*2] ALIGNED16;
int d[N*2] ALIGNED16;

__attribute__((noinline))
void example1 () {
  int i;

  for (i=0; i<256; i++){
    a[i] = b[i] + c[i];
  }
}

__attribute__((noinline))
void example2a (int n, int x) {
   int i;

   /* feature: support for unknown loop bound  */
   /* feature: support for loop invariants  */
   for (i=0; i<n; i++) {
      b[i] = x;
   }
}

__attribute__((noinline))
void example2b (int n, int x) {
  int i = 0;
   /* feature: general loop exit condition  */
   /* feature: support for bitwise operations  */
   while (n--){
      a[i] = b[i]&c[i]; i++;
   }
}


typedef int aint __attribute__ ((__aligned__(16)));
__attribute__((noinline))
void example3 (int n, aint * __restrict__ p, aint * __restrict q) {

   /* feature: support for (aligned) pointer accesses.  */
   while (n--){
      *p++ = *q++;
   }
}

__attribute__((noinline))
void example4a (int n, aint * __restrict__ p, aint * __restrict__ q) {
   /* feature: support for (aligned) pointer accesses  */
   /* feature: support for constants  */
   while (n--){
      *p++ = *q++ + 5;
   }
}

__attribute__((noinline))
void example4b (int n, aint * __restrict__ p, aint * __restrict__ q) {
   int i;

   /* feature: support for read accesses with a compile time known misalignment  */
   for (i=0; i<n; i++){
      a[i] = b[i+1] + c[i+3];
   }
}

__attribute__((noinline))
void example4c (int n, aint * __restrict__ p, aint * __restrict__ q) {
   int i;
    const int MAX = 4;
   /* feature: support for if-conversion  */
   for (i=0; i<n; i++){
      int j = a[i];
      b[i] = (j > MAX ? MAX : 0);
   }
}

__attribute__((noinline))
void  example5 (int n, struct A *s) {
  int i;
  for (i = 0; i < n; i++) {
    /* feature: support for alignable struct access  */
    s->ca[i] = 5;
  }
}

__attribute__((noinline))
void  example7 (int x) {
   int i;

   /* feature: support for read accesses with an unknown misalignment  */
   for (i=0; i<N; i++){
      a[i] = b[i+x];
   }
}

int G[M][N];
__attribute__((noinline))
void example8 (int x) {
   int i,j;

   /* feature: support for multidimensional arrays  */
   for (i=0; i<M; i++) {
     for (j=0; j<N; j++) {
       G[i][j] = x;
     }
   }
}


__attribute__((noinline))
void example9 (unsigned *ret) {
  int i;

  /* feature: support summation reduction.
     note: in case of floats use -funsafe-math-optimizations  */
  unsigned int diff = 0;
  for (i = 0; i < N; i++) {
    diff += (ub[i] - uc[i]);
  }

  *ret = diff;
}


/* feature: support data-types of different sizes.
   Currently only a single vector-size per target is supported; 
   it can accommodate n elements such that n = vector-size/element-size 
   (e.g, 4 ints, 8 shorts, or 16 chars for a vector of size 16 bytes). 
   A combination of data-types of different sizes in the same loop 
   requires special handling. This support is now present in mainline,
   and also includes support for type conversions.  */
__attribute__((noinline))
void example10a(short *__restrict__ sa, short *__restrict__ sb, short *__restrict__ sc, int* __restrict__ ia, int* __restrict__ ib, int* __restrict__ ic) {
  int i;
  for (i = 0; i < N; i++) {
    ia[i] = ib[i] + ic[i];
    sa[i] = sb[i] + sc[i];
  }
}

__attribute__((noinline))
void example10b(short *__restrict__ sa, short *__restrict__ sb, short *__restrict__ sc, int* __restrict__ ia, int* __restrict__ ib, int* __restrict__ ic) {
  int i;
  for (i = 0; i < N; i++) {
    ia[i] = (int) sb[i];
  }
}

/* feature: support strided accesses - the data elements
   that are to be operated upon in parallel are not consecutive - they
   are accessed with a stride > 1 (in the example, the stride is 2):  */
__attribute__((noinline))
void example11() {
   int i;
  for (i = 0; i < N/2; i++){
    a[i] = b[2*i+1] * c[2*i+1] - b[2*i] * c[2*i];
    d[i] = b[2*i] * c[2*i+1] + b[2*i+1] * c[2*i];
  }
}


__attribute__((noinline))
void example12() {
  for (int i = 0; i < N; i++) {
    a[i] = i;
  }
}

__attribute__((noinline))
void example13(int **A, int **B, int *out) {
  int i,j;
  for (i = 0; i < M; i++) {
    int diff = 0;
    for (j = 0; j < N; j+=8) {
      diff += (A[i][j] - B[i][j]);
    }
    out[i] = diff;
  }
}

__attribute__((noinline))
void example14(int **in, int **coeff, int *out) {
  int k,j,i=0;
  for (k = 0; k < K; k++) {
    int sum = 0;
    for (j = 0; j < M; j++)
      for (i = 0; i < N; i++)
          sum += in[i+k][j] * coeff[i][j];

    out[k] = sum;
  }

}


__attribute__((noinline))
void example21(int *b, int n) {
  int i, a = 0;

  for (i = n-1; i >= 0; i--)
    a += b[i];

  b[0] = a;
}

__attribute__((noinline))
void example23 (unsigned short *src, unsigned int *dst)
{
  int i;

  for (i = 0; i < 256; i++)
    *dst++ = *src++ << 7;
}


__attribute__((noinline))
void example24 (short x, short y)
{
  int i;
  for (i = 0; i < N; i++)
    ic[i] = fa[i] < fb[i] ? x : y;
}


__attribute__((noinline))
void example25 (void)
{
  int i;
  char x, y;
  for (i = 0; i < N; i++)
    {
      x = (da[i] < db[i]);
      y = (dc[i] < dd[i]);
      dj[i] = x & y;
    }
}

void init_memory(void *start, void* end) {
  unsigned char state = 1;
  while (start != end) {
    state *= 7; state ^= 0x27; state += 1;
    *((unsigned char*)start) = state;
    start = ((char*)start) + 1;
  }
}

void init_memory_float(float *start, float* end) {
  float state = 1.0;
  while (start != end) {
    state *= 1.1;
    *start = state;
    start++;
  }
}

unsigned digest_memory(void *start, void* end) {
  unsigned state = 1;
  while (start != end) {
    state *= 3;
    state ^= *((unsigned char*)start);
    state = (state >> 8  ^ state << 8);
    start = ((char*)start) + 1;
  }
  return state;
}

class Timer {

public:
  Timer(const char* title, bool print) {
    Title = title;
    Print = print;
    gettimeofday(&Start, 0);
  }

  ~Timer() {
    gettimeofday(&End, 0);
    long mtime, s,us;
    s = End.tv_sec  - Start.tv_sec;
    us = End.tv_usec - Start.tv_usec;
    mtime = (s*1000 + us/1000.0)+0.5;
    if (Print)
      std::cout<<Title<<", "<<mtime<<", msec\n";
  }

private:
  const char* Title;
  bool Print;
  struct timeval Start, End;
};


// Warmup and then measure.
#define BENCH(NAME, RUN_LINE, ITER, DIGEST_LINE) {\
  RUN_LINE;\
  Timer atimer(NAME, print_times);\
  for (int i=0; i < (ITER); ++i) RUN_LINE;\
  unsigned r = DIGEST_LINE;\
  results.push_back(r);\
 }

int main(int argc,char* argv[]){

  bool print_times = argc > 1;

  std::vector<unsigned> results;
  unsigned dummy = 0;
#ifdef SMALL_PROBLEM_SIZE
  const int Mi = 1<<10;
#else
  const int Mi = 1<<18;
#endif
  init_memory(&ia[0], &ia[N]);
  init_memory(&ib[0], &ib[N]);
  init_memory(&ic[0], &ic[N]);
  init_memory(&sa[0], &sa[N]);
  init_memory(&sb[0], &sb[N]);
  init_memory(&sc[0], &sc[N]);
  init_memory(&a[0], &a[N*2]);
  init_memory(&b[0], &b[N*2]);
  init_memory(&c[0], &c[N*2]);
  init_memory(&ua[0], &ua[N]);
  init_memory(&ub[0], &ub[N]);
  init_memory(&uc[0], &uc[N]);
  init_memory(&G[0][0], &G[0][N]);
  init_memory_float(&fa[0], &fa[N]);
  init_memory_float(&fb[0], &fb[N]);
  init_memory_float(&da[0], &da[N]);
  init_memory_float(&db[0], &db[N]);
  init_memory_float(&dc[0], &dc[N]);
  init_memory_float(&dd[0], &dd[N]);

  BENCH("Example1",   example1(), Mi*10, digest_memory(&a[0], &a[256]));
  BENCH("Example2a",  example2a(N, 2), Mi*4, digest_memory(&b[0], &b[N]));
  BENCH("Example2b",  example2b(N, 2), Mi*2, digest_memory(&a[0], &a[N]));
  BENCH("Example3",   example3(N, ia, ib), Mi*2, digest_memory(&ia[0], &ia[N]));
  BENCH("Example4a",  example4a(N, ia, ib), Mi*2, digest_memory(&ia[0], &ia[N]));
  BENCH("Example4b",  example4b(N-10, ia, ib), Mi*2, digest_memory(&ia[0], &ia[N]));
  BENCH("Example4c",  example4c(N, ia, ib), Mi*2, digest_memory(&ib[0], &ib[N]));
  BENCH("Example7",   example7(4), Mi*4, digest_memory(&a[0], &a[N]));
  BENCH("Example8",   example8(8), Mi/4, digest_memory(&G[0][0], &G[0][N]));
  BENCH("Example9",   example9(&dummy), Mi*2, dummy);
  BENCH("Example10a", example10a(sa,sb,sc,ia,ib,ic), Mi*2, digest_memory(&ia[0], &ia[N]) + digest_memory(&sa[0], &sa[N]));
  BENCH("Example10b", example10b(sa,sb,sc,ia,ib,ic), Mi*4, digest_memory(&ia[0], &ia[N]));
  BENCH("Example11",  example11(), Mi*2, digest_memory(&d[0], &d[N]));
  BENCH("Example12",  example12(), Mi*4, digest_memory(&a[0], &a[N]));
  //BENCH("Example21",  example21(ia, N), Mi*4, digest_memory(&ia[0], &ia[N]));
  BENCH("Example23",  example23(usa,ua), Mi*8, digest_memory(&usa[0], &usa[256]));
  BENCH("Example24",  example24(2,4), Mi*2, 0);
  BENCH("Example25",  example25(), Mi*2, digest_memory(&dj[0], &dj[N]));

  std::cout<<std::hex;
  std::cout<<"Results: ("<<std::accumulate(results.begin(), results.end(), 0)<<"):";
  for (unsigned i=0; i < results.size(); ++i) {
    std::cout<<" "<<results[i];
  }
  std::cout<<"\n";

  return 0;
}



