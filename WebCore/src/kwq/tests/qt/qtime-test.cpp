#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qdatetime.h>

int main() {

    QTime d0;
    QTime d1 = QTime(23, 59, 59, 999);
    QTime d2;

    cout << d1 << endl;

    d0 = d1;
    cout << "assignment: " << d0 << endl;

    d0 = d1.addSecs(1);
    cout << "day roll (seconds): " << d0 << endl;

    d0 = d1;
    d0 = d1.addMSecs(1);
    cout << "day roll (milli-seconds): " << d0 << endl;

    d2 = d1;
    cout << "d0 is: " << d0 << endl;
    cout << "d1 is: " << d1 << endl;
    cout << "d2 is: " << d2 << endl;
    cout << "(d0 < d1): " << (d0 < d1) << endl;
    cout << "(d0 <= d1): " << (d0 <= d1) << endl;
    cout << "(d0 > d1): " << (d0 > d1) << endl;
    cout << "(d0 >= d1): " << (d0 >= d1) << endl;
    cout << "(d2 < d1): " << (d2 < d1) << endl;
    cout << "(d2 <= d1): " << (d2 <= d1) << endl;
    cout << "(d2 > d1): " << (d2 > d1) << endl;
    cout << "(d2 >= d1): " << (d2 >= d1) << endl;
    
    return 0;
}
