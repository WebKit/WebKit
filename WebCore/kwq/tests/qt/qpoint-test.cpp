#include <iostream>

#include <qpoint.h>

int main() {

    QPoint p0;
    QPoint p1 = QPoint(1,1);
    QPoint p2 = QPoint(3,3);
    QPoint p3 = p1 + p2;
    QPoint p4 = p3;
    QPoint p5 = p3;

    p5.setX(10);
    p5.setY(10);
    p5 += p1;
    p5 -= p2;
    p5 *= 3;
    p5 /= 2;
    
    cout << p1 << endl;
    cout << p2 << endl;
    cout << p3 << endl;
    cout << p0.isNull() << endl;
    cout << p1.isNull() << endl;
    cout << (p1 == p4) << endl;
    cout << (p3 == p4) << endl;
    cout << (p1 != p4) << endl;
    cout << (p3 != p4) << endl;
    cout << p5 << endl;

    return 0;
}
