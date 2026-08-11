/* Standard Container Benchmark 


   Version 0.9, May 23, 2003 


The primary purpose of this benchmark is to show how different standard 
containers are in terms of performance. We have observed that programmers 
often use sets, multisets, deques in the situations where sorted vectors 
are the optimal solutions. Similarly, programmers often choose lists simply 
because they do a few insertions and deletions without knowing that vectors 
are more efficient at that for small containers. 


Frequently, of course, performance does not matter, 
but it is important that programmers are aware of the consequences of their 
choices. We are not saying that only vectors should be used, there are 
cases when one really needs a more complicated data structure, but one needs to 
understand the price for additional functionality. 


The secondary purpose of the benchmark is to encourage compiler and library vendors to 
keep improving performance. For example, it is not acceptable that some compilers give 
you a sizable penalty for using vector iterators instead of pointer. It is also quite 
clear that  performance of other standard containers could be improved. 


The benchmark is doing the same task 7 times using different data structures: 
array, vector (using a pointer as iterator), vector (using the defailt cector iterator), 
list, deque, set and multiset. The task is to remove duplicates from a sequence of doubles. 
This is a simple test, but it illustrates the performance of containers. 


It is clear that the benchmark needs to be eventually extended 
to slists and even to hash-sets and hash-maps, but we decided that at present it 
should work only with the standard containers. As the standard grows, so can 
the benchmark. The additional benefit of not including hash based containers is 
that now all the test have identical asymptotic complexity and, even more 
importantly, do almost the same number of comparisons. The difference is only 
in data structure overhead and cache behavior. 


The number of times that a given test is run inversly proportional to NlogN where N is the 
size of the sequence.  This allows one to see how containers of different size behave. 
The instructive values used for the benchmark are: 10, 100, 1000, 10000, 100000, 1000000. 


The time taken for a run of the benchmark depends on the machine used, the compiler used, 
the compiler and optimizer settings used, as well as the standard library. Please note that 
the time taken could be several minutes - and much more if you use a slow debug mode. 


The output is a table where columns are separated by tabs and rows by newlines. This is 
barely ok for a human reader, but ideal for feeding into a program, such as a spreadsheet 
for display or analysis. 


If you want to add your own test of a container, add the name of your container to 
the "header string, write a test function like the existing ones, e.g. vector_test, 
and add a call of "run" for your test in "run_tests". 


Alex Stepanov 
stepa...@adobe.com 


Bjarne Stroustrup 
b...@cs.tamu.edu 


*/ 


#include <stddef.h>       // some older implementations lack <cstddef> 
#include <time.h> 
#include <math.h> 
#include <stdlib.h> 


#include <vector> 
#include <algorithm> 
#include <list> 
#include <deque> 
#include <set> 


#include <iostream> 
#include <iomanip> 


typedef double element_t; 


using namespace std; 


vector<double> result_times; // results are puched into this vector 


typedef void(*Test)(element_t*, element_t*, int); 
void run(Test f, element_t* first, element_t* last, int number_of_times) 
{ 
        while (number_of_times-- > 0) f(first,last,number_of_times); 
} 


void array_test(element_t* first, element_t* last, int number_of_times) 
{ 
       element_t* array = new element_t[last - first]; 
       copy(first, last, array); 
       sort(array, array + (last - first)); 
       unique(array, array + (last - first)); 
       delete [] array;   


} 


void vector_pointer_test(element_t* first, element_t* last, int number_of_times) 
{ 
       vector<element_t> container(first, last); 
           // &*container.begin() gets us a pointer to the first element 
       sort(&*container.begin(), &*container.end()); 
       unique(&*container.begin(), &*container.end()); 


} 


void vector_iterator_test(element_t* first, element_t* last, int number_of_times) 
{ 
       vector<element_t> container(first, last); 
       sort(container.begin(), container.end()); 
       unique(container.begin(), container.end()); 


} 


void deque_test(element_t* first, element_t* last, int number_of_times) 
{   
//       deque<element_t> container(first, last); CANNOT BE USED BECAUSE OF MVC++ 6 
        deque<element_t> container(size_t(last - first), 0.0); 
        copy(first, last, container.begin()); 
        sort(container.begin(), container.end()); 
        unique(container.begin(), container.end()); 


} 


void list_test(element_t* first, element_t* last, int number_of_times) 
{ 
       list<element_t> container(first, last); 
       container.sort(); 
           container.unique(); 


} 


void set_test(element_t* first, element_t* last, int number_of_times) 
{ 
       set<element_t> container(first, last); 


} 


void multiset_test(element_t* first, element_t* last, int number_of_times) 
{ 
       multiset<element_t> container(first, last); 
       typedef multiset<element_t>::iterator iterator; 
           { 
                        iterator first = container.begin(); 
                        iterator last = container.end(); 

                        while (first != last) { 
                                iterator next = first; 
                                if (++next == last) break; 
                                if (*first == *next) 
                                        container.erase(next); 
                                else 
                                        ++first; 
                        } 
           } 



} 


void initialize(element_t* first, element_t* last) 
{ 
  element_t value = 0.0; 
  while (first != last) { 
         *first++ = value; 
         value += 1.; 
  } 


} 


double logtwo(double x) 
{ 
  return log(x)/log((double) 2.0); 


} 


const int largest_size = 1000000; 

int number_of_tests(int size) { 
        double n = size; 
        double largest_n = largest_size; 
        return int(floor((largest_n * logtwo(largest_n)) / (n * logtwo(n)))); 



} 


void run_tests(int size) 
{ 
        const int n = number_of_tests(size); 
        const size_t length = 2*size; 
        result_times.clear(); 

// make a random test set of the chosen size: 
  vector<element_t> buf(length); 
  element_t* buffer = &buf[0]; 
  element_t* buffer_end = &buf[length]; 
  initialize(buffer, buffer + size);            // elements 
  initialize(buffer + size, buffer_end);        // duplicate elements 
  random_shuffle(buffer, buffer_end); 


// test the containers: 
  run(array_test, buffer, buffer_end, n); 
  run(vector_pointer_test, buffer, buffer_end, n); 
  run(vector_iterator_test, buffer, buffer_end, n); 
  run(deque_test, buffer, buffer_end, n); 
  run(list_test, buffer, buffer_end, n); 
  run(set_test, buffer, buffer_end, n); 
  run(multiset_test, buffer, buffer_end, n); 



} 


int main() 
{ 
  const int sizes [] = { 100000 }; 
  const int n = sizeof(sizes)/sizeof(int); 
  for (int i = 0; i < n; ++i) run_tests(sizes[i]);
}