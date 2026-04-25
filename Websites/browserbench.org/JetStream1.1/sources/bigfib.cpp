// ###########################################
// [C++] Computing very long Fibonacci numbers
//       Version 2.5.1 (with performance test)
// -------------------------------------------
// Created by Alex Vinokur
// http://up.to/alexvn
// ###########################################

// http://groups.google.com/groups?selm=bo4nls%2417vfq6%241%40ID-79865.news.uni-berlin.de

#include <climits>
#include <cstdlib>
#include <cassert>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
using namespace std;


#define MAX_VALUE(x,y)  ((x) > (y) ? (x) : (y))
#define ASSERT(x)
// #define ASSERT(x)    assert(x)


#define MAX_UNIT_VALUE  (ULONG_MAX >> 2)
#define BASE1  10
#define BASE2  1000000000   // BASE1 ** (BASE1 - 1)

#if (BASE2 >= MAX_UNIT_VALUE)
#error Compilation Error-1 : (BASE2 >= MAX_UNIT_VALUE)
#endif

#if (!(BASE1 * (BASE2/BASE1 + 1) < MAX_UNIT_VALUE))
#error Compilation Error-2 : (!(BASE1 * (BASE2/BASE1 + 1) < MAX_UNIT_VALUE))
#endif


typedef unsigned int  uint;
typedef unsigned long ulong;

// =========
class BigInt
// =========
{
friend ostream& operator<< (ostream& os, const BigInt& ins_i);

  private :
    static ulong  head_s;
    vector<ulong> units_;

  public :
    BigInt (ulong unit_i)
    {
      ASSERT (unit_i < BASE2);
      units_.push_back (unit_i);
    }

    BigInt (BigInt big1_i, BigInt big2_i)
    {
      const ulong max_size = MAX_VALUE (big1_i.units_.size (), big2_i.units_.size ());

      big1_i.units_.resize(max_size);
      big2_i.units_.resize(max_size);
      units_.resize(max_size);

      head_s = 0;
      transform (big1_i.units_.begin(), big1_i.units_.end(), big2_i.units_.begin(), units_.begin(), *this);

      if (head_s) units_.push_back (head_s);

    }

    ulong operator() (const ulong n1, const ulong n2)
    {
      const ulong value = n1 + n2 + head_s;
      head_s = value/BASE2;
      return (value%BASE2);
    } 
};


// --------------
inline ostream& operator<< (ostream& os, const BigInt& ins_i)
{
  ASSERT (!ins_i.units_.empty ());
  for (ulong i = (ins_i.units_.size () - 1); i; --i)
  {
    os << ins_i.units_ [i] << setw (BASE1 - 1) << setfill ('0');
  } return os << ins_i.units_ [0];
}


// ============
class Fibonacci
// ============
{
  private :
    vector<BigInt>   fibs_;
    BigInt           get_number (uint n_i = 0);

  public :
    void             show_all_numbers () const;
    void             show_last_number () const;
    void             show_number (ulong n_i);

    Fibonacci (uint n_i = 0) { get_number (n_i); }
    ~Fibonacci () {}

};


// -----------------------
BigInt Fibonacci::get_number (uint n_i)
{
fibs_.reserve(n_i + 1);
const uint cur_size = fibs_.size ();

  for (uint i = cur_size; i <= n_i; ++i)
  {
    switch (i)
    {
      case 0 :
        fibs_.push_back (BigInt(0));
        break;

      case 1 :
        if (fibs_.empty ()) fibs_.push_back (BigInt (0));
        fibs_.push_back(BigInt (1));
        break;

      default :
        fibs_.push_back (BigInt (get_number (i - 2), get_number (i - 1)));
        break;
    }
  }

  ASSERT (n_i < fibs_.size());
  return fibs_ [n_i];

}


// -----------------------
void Fibonacci::show_all_numbers () const
{
ostringstream   oss;

  for (uint i = 0; i < fibs_.size (); ++i)
  {
    oss << "Fib [" << i << "] = " << fibs_ [i] << "\n";
  } cout << oss.str();
}


// -----------------------
void Fibonacci::show_last_number () const
{
ostringstream   oss;

  oss << "Fib [" << (fibs_.size() - 1) << "] = " << fibs_.back() << "\n";

  cout << oss.str();
}



// -----------------------
void Fibonacci::show_number (ulong n_i)
{
ostringstream   oss;

  if (!(n_i < fibs_.size())) get_number (n_i);

  oss << "Fib [" << n_i << "] = " << fibs_[n_i] << "\n";

  cout << oss.str();
}

// -------------------
ulong BigInt::head_s (0);




// ==============================
#define ALL_FIBS  "all"
#define TH_FIB    "th"
#define SOME_FIBS "some"
#define RAND_FIBS "rand"

#define MAX_RAND_FIB   25000

#define SETW1      4

// ---------------------
void usage (char **argv)
{
  argv[0] = "bigfib";
  cerr << "USAGE : "
       << endl

       << "  "
       << argv[0]
       << " "
       << setw (SETW1)
       << std::left
       << ALL_FIBS
       << " <N>              ---> Fibonacci [0 - N]"
       << endl

       << "  "
       << argv[0]
       << " "
       << std::left
       << setw (SETW1)
       << TH_FIB
       << " <N>              ---> Fibonacci [N]"
       << endl

       << "  "
       << argv[0]
       << " "
       << std::left
       << setw (SETW1)
       << SOME_FIBS
       << " <N1> [<N2> ...]  ---> Fibonacci [N1], Fibonacci [N2], ..."
       << endl

       << "  "
       << argv[0]
       << " "
       << std::left
       << setw (SETW1)
       << RAND_FIBS
       << " <K>  [<M>]       ---> K random Fibonacci numbers ( < M; Default = "
       << MAX_RAND_FIB
       << " )"
       << endl;
}


// ---------------------
string check (int argc, char **argv)
{
  if (argc < 3) return string();

const string str (argv[1]);
  if (
       (str == ALL_FIBS)
       ||
       (str == TH_FIB)
       ||
       (str == SOME_FIBS)
       ||
       (str == RAND_FIBS)
     )
  {
    return str;
  }
  return string();

}


// ---------------------

int main (int argc, char **argv)
{
  string option (check (argc, argv));
  uint N;
  if (option.empty()) {
    // usage (argv);
    // return 1;
    option = TH_FIB;
#ifdef SMALL_PROBLEM_SIZE
    N = 15000;
#else
    N = 50000;
#endif
  } else {
    N = atoi (argv[2]);
  }

  if (option == ALL_FIBS)
  {
    Fibonacci fib(N);
    fib.show_all_numbers();
  }

  if (option == TH_FIB)
  {
    Fibonacci fib(N);
    fib.show_last_number();
  }

  if (option == SOME_FIBS)
  {
    Fibonacci fib;
    for (int i = 2; i < argc; i++) fib.show_number (atoi(argv[i]));
  }

  if (option == RAND_FIBS)
  {
    const int max_rand_fib = (argc == 3) ? MAX_RAND_FIB : atoi (argv[3]);
    Fibonacci fib;
    for (uint i = 0; i < N; i++) fib.show_number (rand()%max_rand_fib);
  }

  return 0;
}



