#include <iostream>

#include <qrect.h>

int main() {

    QRect r0;
    QRect r1 = QRect(0,0,100,100);
    QRect r2 = QRect(100,100,200,200);
    QRect r3 = r1.unite(r2);
    QRect r4 = r3;
    QRect r5 = r3;

    r5.setLeft(10);
    r5.setTop(10);
    r5.setRight(20);
    r5.setBottom(20);
        
    cout << r1 << endl;
    cout << r2 << endl;
    cout << r3 << endl;
    cout << r0.isNull() << endl;
    cout << r1.isNull() << endl;
    cout << (r1 == r4) << endl;
    cout << (r3 == r4) << endl;
    cout << (r1 != r4) << endl;
    cout << (r3 != r4) << endl;
    cout << r5 << endl;

    return 0;
}
