#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qpoint.h>

int main() {

    QPoint p0;
    QPoint p1 = QPoint(1,1);
    QPoint p2 = QPoint(3,3);
    QPoint p3 = p1 + p2;
    QPoint p4 = p3;
    QPoint p5 = p3;
    QPoint p6 = p2 - p1;
    
    cout << "p1: " << p1 << endl;
    cout << "p2: " << p2 << endl;
    cout << "p3 = p1 + p2: " << p3 << endl;
    cout << "p4 = p3: " << p4 << endl;
    cout << "p5 = p3: " << p5 << endl;
    cout << "p6 = p2 - p1: " << p6 << endl;
    
    p5.setX(10);
    cout << "p5 setX(10): " << p5 << endl;
    
    p5.setY(10);
    cout << "p5 setY(10): " << p5 << endl;
    
    p5 += p1;
    cout << "p5 += p1: " << p5 << endl;
    
    p5 -= p2;
    cout << "p5 -= p2: " << p5 << endl;
    
    p5 *= 3;
    cout << "p5 *= 3: " << p5 << endl;
    p5 *= 3.333;
    cout << "p5 *= 3.333: " << p5 << endl;
    
    p5 /= 2;
    cout << "p5 /= 2: " << p5 << endl;
    p5 /= 2.222;
    cout << "p5 /= 2.222: " << p5 << endl;    
    
    cout << "p5 manhattanLength: " << p5.manhattanLength() << endl;
    
    cout << "p5 x: " << p5.x() << endl;
    cout << "p5 y: " << p5.y() << endl;
    
    
    cout << "p0 isNull: " << p0.isNull() << endl;
    cout << "p1 isNull: " << p1.isNull() << endl;
    
    cout << "p1 == p4: " << (p1 == p4) << endl;
    cout << "p3 == p4: " << (p3 == p4) << endl;
    
    cout << "p1 != p4: " << (p1 != p4) << endl;
    cout << "p3 != p4: " << (p3 != p4) << endl;

    cout << "p5 * 2: " << (p5 * 2) << endl;
    cout << "3 * p5: " << (3 * p5) << endl;
    cout << "p5 / 2.222: " << (p5 / 2.222) << endl;
    cout << "p5 / 3: " << (p5 / 3) << endl;    
    cout << "p5 * 2.222: " << (p5 * 2.222) << endl;
    cout << "3.333 * p5: " << (3.333 * p5) << endl;
        
    return 0;
}
