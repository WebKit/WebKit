#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qvector.h>

int main() {

    QVector<int> v0;
    QVector<int> v1 = QVector<int>(10);
    QVector<int> v2 = v1;
    
    int i = 0;
    int j = 1;
    int k = 2;
    int l = 3;
    int m = 4;
    int n = 5;
    int o = 6;
    int *x;
    
    v1.insert(0,&i);
    v1.insert(1,&j);
    v1.insert(2,&k);
    v1.insert(3,&l);
    v1.insert(4,&m);
    v1.insert(5,&n);
    v1.insert(6,&o);
        
    cout << "v1: " << v1 << endl;
    cout << "v2 = v1: " << v2 << endl;
    
    cout << "v1 find 2: " << v1.find(&k) << endl;

    cout << "v1 contains 3: " << v1.containsRef(&l) << endl;
    cout << "v1 contains 0: " << v1.containsRef(&i) << endl;
    
    cout << "v1 count: " << v1.count() << endl;
    cout << "v1 size: " << v1.size() << endl;
    
    v1.remove(6);
    cout << "v1 remove 6: " << v1 << endl;

    v1.resize(15);
    cout << "v1 resize 15: " << v1.size() << endl;
    
    x = v1.at(2);
    cout << "v1 at 2: " << *x << endl;
    
    x = v1[3];
    cout << "v1[3]: " << *x << endl;
    
    v2.clear();
    cout << "v2 clear: " << v2 << endl;
    
    cout << "v0: " << v0.isEmpty() << endl;
    cout << "v1: " << v1.isEmpty() << endl;
    cout << "v2: " << v2.isEmpty() << endl;
    
    return 0;
}
