#include <iostream>

#include <qmap.h>

int main() {

    QMap<int,int> d0;
    QMap<int,int> d1;

    d1.insert(1, 10);
    d1.insert(2, 20);
    d1.insert(3, 30);
    d1.insert(4, 40);
    d1.insert(5, 50);

    cout << d1 << endl;

    cout << "item keyed to 'one': " << *(d1.find(1)) << endl;

    d1.remove(3);
    cout << "remove item keyed to 'three': " << d1 << endl;

    cout << "d0 is empty: " << d0.isEmpty() << endl;
    cout << "d1 is empty: " << d1.isEmpty() << endl;

    return 0;
}
