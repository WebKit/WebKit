#include <iostream>

#include <qvaluelist.h>

int main() {

    QValueList<int> p0;
    QValueList<int> p1;

    p0.append(0);
    p0.append(1);
    p0.append(2);
    p0.append(3);
    p0.append(4);
    p0.append(5);
        
    cout << "p0: " << p0 << endl;
    cout << "p0[3]: " << p0[3] << endl;
    p0 += 6;
    p0 += 7;
    cout << "p0 += 6,7: " << p0 << endl;

    cout << "p0 iterator: ";
    QValueListIterator<int> it = p0.find(4);
    while (it != p0.end()) {
        cout << *it;
        if (++it != p0.end()) {
            cout << ", ";
        }
    }
    cout << endl;

    cout << "p0 count: " << p0.count() << endl;
    
    p0.remove(7);
    cout << "p0 remove 7: " << p0 << endl;

    cout << "p0 contains 7: " << p0.contains(7) << endl;
    cout << "p0 remove 6: " << p0.contains(6) << endl;
    
    cout << "p0 first 6: " << p0.first() << endl;
    cout << "p0 last 6: " << p0.last() << endl;
    
    p1 = p0;
    cout << "p1 = p0: " << p1 << endl;

    cout << "p0 isEmpty: " << p0.isEmpty() << endl;
    p0.clear();
    cout << "p0 clear: " << p0 << endl;
    cout << "p0 isEmpty: " << p0.isEmpty() << endl;
    
    
    
    
    return 0;
}
