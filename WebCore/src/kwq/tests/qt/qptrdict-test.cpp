#include <iostream>

#include <qstring.h>
#include <qptrdict.h>

int main() {

    QPtrDict<int> d0;
    QPtrDict<int> d1;

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

    cout << "item keyed to 1: " << *(d1.find(&v1)) << endl;
    cout << "item keyed to 2: " << *(d1.find(&v2)) << endl;
    cout << "item keyed to 3: " << *(d1.find(&v3)) << endl;
    cout << "item keyed to 4: " << *(d1.find(&v4)) << endl;
    cout << "item keyed to 5: " << *(d1.find(&v5)) << endl;

    d1.remove(&v2);
    cout << "item keyed to 2: " << d1.find(&v2) << endl;

    cout << "d0 is empty: " << d0.isEmpty() << endl;
    cout << "d1 is empty: " << d1.isEmpty() << endl;

    return 0;
}
