#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qmap.h>

int main() {

    QMap<int,int> d0;
    QMap<int,int> d1;
    QMap<int,int> d2;
    
    d1.insert(1, 10);
    d1.insert(2, 20);
    d1.insert(3, 30);
    d1.insert(4, 40);
    d1.insert(5, 50);

    cout << "d1: " << d1 << endl;
    cout << "d1 count: " << d1.count() << endl;
    
    cout << "d1 find 1: " << *(d1.find(1)) << endl;

    d1.remove(3);
    cout << "d1 remove 3: " << d1 << endl;

    cout << "d0 isEmpty: " << d0.isEmpty() << endl;
    cout << "d1 isEmpty: " << d1.isEmpty() << endl;

    d2 = d1;
    cout << "d2 = d1, d2: " << d2 << endl;
    
    cout << "d2[1]: " << d2[1] << endl;
    
    d1.clear();
    cout << "d1 clear: " << d1 << endl;
    return 0;
}
