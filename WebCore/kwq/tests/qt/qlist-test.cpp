#include <iostream>

#include <stdlib.h>
#include <string.h>

#include <qlist.h>

int main() {

    QList<int> p0;

    int i = 0;
    int j = 1;
    int k = 2;
    int l = 3;
    int m = 4;
    int n = 5;
    int o = 6;

    p0.append(&i);
    p0.append(&j);
    p0.append(&k);
    p0.append(&l);
    p0.append(&m);
    p0.append(&n);
    p0.append(&o);
        
    cout << p0 << endl;

    cout << "finding index of 2: " << p0.find(&k) << endl;

    p0.removeFirst();
    cout << p0 << endl;
    
    p0.removeLast();
    cout << p0 << endl;
    
    cout << "taking last item: " << *(p0.take()) << endl;
    cout << p0 << endl;

    cout << "contains 3: " << p0.containsRef(&l) << endl;
    cout << "contains 0: " << p0.containsRef(&i) << endl;
    
    return 0;
}
