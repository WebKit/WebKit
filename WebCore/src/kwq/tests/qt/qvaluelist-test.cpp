#include <iostream>

#include <qvaluelist.h>

int main() {

    QValueList<int> p0;

    p0.append(0);
    p0.append(1);
    p0.append(2);
    p0.append(3);
    p0.append(4);
    p0.append(5);
        
    cout << p0 << endl;
    cout << p0[3] << endl;
    p0 += 6;
    p0 += 7;
    cout << p0 << endl;

    QValueListIterator<int> it = p0.find(4);
    while (it != p0.end()) {
        cout << *it;
        if (++it != p0.end()) {
            cout << ", ";
        }
    }
    cout << endl;

    return 0;
}
