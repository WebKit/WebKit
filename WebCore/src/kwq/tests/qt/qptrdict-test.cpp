#include <iostream>

#include <qstring.h>
#include <qptrdict.h>

int main() {

    QPtrDict<int> d0;
    QPtrDict<int> d1;
    QPtrDict<int> d2;

    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int v5 = 5;
    
    d1.insert(&v1, &v1);
    d1.insert(&v2, &v2);
    d1.insert(&v3, &v3);
    d1.insert(&v4, &v4);
    d1.insert(&v5, &v5);

    
    //cout << "d1: " << d1 << endl;
    
    cout << "d1 find 1: " << *(d1.find(&v1)) << endl;
    cout << "d1 find 2: " << *(d1.find(&v2)) << endl;
    cout << "d1 find 3: " << *(d1.find(&v3)) << endl;
    cout << "d1 find 4: " << *(d1.find(&v4)) << endl;
    cout << "d1 find 5: " << *(d1.find(&v5)) << endl;

    int *n2;
    n2 = d1.take(&v4);                      
    cout << "d1 take 4: " << *n2 << endl;

    d1.remove(&v2);
    //cout << "d1 remove 2: " << d1 << endl;
    cout << "d1 find 2: " << d1.find(&v2) << endl;

    cout << "d0 is empty: " << d0.isEmpty() << endl;
    cout << "d1 is empty: " << d1.isEmpty() << endl;

    cout << "d1 count: " << d1.count() << endl;

    d2 = d1;
    //cout << "d2 = d1: " << d2 << endl;
    
    d1.clear();
    //cout << "d1 clear: " << d1 << endl;
    return 0;
}
