#include <iostream>

#include <qvector.h>

int main() {

    QVector<int> v0 = QVector<int>(10);

    int i = 0;
    int j = 1;
    int k = 2;
    int l = 3;
    int m = 4;
    int n = 5;
    int o = 6;

    v0.insert(0,&i);
    v0.insert(1,&j);
    v0.insert(2,&k);
    v0.insert(3,&l);
    v0.insert(4,&m);
    v0.insert(5,&n);
    v0.insert(6,&o);
        
    cout << v0 << endl;

    cout << "finding index of 2: " << v0.find(&k) << endl;

    cout << "contains 3: " << v0.containsRef(&l) << endl;
    cout << "contains 0: " << v0.containsRef(&i) << endl;
    
    return 0;
}
