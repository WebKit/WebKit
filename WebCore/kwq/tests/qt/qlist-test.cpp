#include <iostream>

#include <stdlib.h>
#include <string.h>

#include <qlist.h>

int main() {

    QList<int> p0;
    QList<int> p1;
    QList<int> p2;
    QList<int> p3;
    
    int i = 0;
    int j = 1;
    int k = 2;
    int l = 3;
    int m = 4;
    int n = 5;
    int o = 6;
    int p = 7;
    int q = 8;
    int r = 9;
    int s = 10;

    p1.append(&i);
    p1.append(&j);
    p1.append(&k);
    p1.append(&l);
    p1.append(&m);
    p1.append(&n);
    p1.append(&o);
    
    cout << "p0: " << p0 << endl;
    cout << "p1: " << p1 << endl;
   
    cout << "p0 isEmpty: " << p0.isEmpty() << endl;    
    cout << "p1 isEmpty: " << p1.isEmpty() << endl;

    cout << "p0 count: " << p0.count() << endl;    
    cout << "p1 count: " << p1.count() << endl;    
    
    cout << "p1 find index of 2: " << p1.find(&k) << endl;

    p1.removeFirst();
    cout << "p1 removeFirst: " << p1 << endl;
    
    p1.removeLast();
    cout << "p1 removeLast: " << p1 << endl;
    
    cout << "p1 take: " << *(p1.take()) << " " << p1 << endl;

    cout << "p1 containsRef 3: " << p1.containsRef(&l) << endl;
    cout << "p1 containsRef 0: " << p1.containsRef(&i) << endl;
    
    p2.append(&o);
    p2.append(&n);
    p2.append(&m);    
    p2.append(&l);
    p2.append(&k);   
    p2.append(&j);         
    p2.append(&i);
    cout << "p2 unsorted: " << p2 << endl;
    p2.sort();
    cout << "p2 sorted: " << p2 << endl; //heh?

    int *n1;
    n1 = p1.at(2);
    cout << "p1 at 2: "<< *n1 << endl;
    
    p1.insert(1, &p);
    p1.insert(2, &q);
    p1.insert(3, &r);
    p1.insert(4, &s);
    cout << "p1 insert 7 in 1: " << p1 << endl;
    
    p1.remove();
    cout << "p1 remove: " << p1 << endl;
    
    p1.remove(&p);
    cout << "p1 remove 7: " << p1 << endl;

    p1.removeRef(&q);
    cout << "p1 removeRef 8: " << p1 << endl;
    
    int *n2;
    n2 = p2.getLast();
    cout << "p2 getLast: "<< *n2 << endl;
    n2 = p2.current();
    cout << "p2 current: "<< *n2 << endl;
    n2 = p2.first();
    cout << "p2 first: "<< *n2 << endl;
    n2 = p2.last();
    cout << "p2 last: "<< *n2 << endl;
    n2 = p2.prev();
    cout << "p2 prev: "<< *n2 << endl;
    n2 = p2.next();
    cout << "p2 next: "<< *n2 << endl;
    
    p2.prepend(&r);
    cout << "p2 prepend 9: "<< p2 << endl;
    
    cout << "p1: "<< p1 << endl;
    p1.setAutoDelete(FALSE);
    p1.clear();
    cout << "p1 clear (autoDelete off): "<< p1 << endl; // heh?
    p1.setAutoDelete(TRUE);
    p1.clear();
    cout << "p1 clear (autoDelete on): "<< p1 << endl;   
    
    p3 = p2;
    cout << "p3 = p2: "<< p3 << endl;
    return 0;
}
