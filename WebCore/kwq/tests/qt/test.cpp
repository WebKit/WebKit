#include <iostream>

#if 0
#include "qsize.h"
#include "qrect.h"
#include "qcstring.h"
#endif 

#include "qstring.h"

int main() {

#if 0    
    QSize s1 = QSize(3,3);
    QSize s2 = QSize(s1);
    cout << (s1 == s2) << endl;
    s2.setHeight(5);
    cout << (s1) << endl;
    cout << (s2) << endl;

    cout << "--------------------------------------------" << endl;

    QRect r1 = QRect(5,5,10,10);
    QRect r2 = QRect(20,20,10,10);
    QRect r3 = r1.intersect(r2);

    cout << (r1) << endl;
    cout << (r2) << endl;
    cout << (r3) << endl;
    cout << (r1.intersects(r2)) << endl;
    cout << (r1.intersects(r3)) << endl;

    cout << "--------------------------------------------" << endl;
#endif

    QString s = "foo";
    
    cout << s << endl;
}
