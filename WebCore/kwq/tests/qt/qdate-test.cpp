#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qdatetime.h>

int main() {

    QDate d0;
    QDate d1(1999, 12, 31);
    QDate d2(2001, 10, 23);

    cout << "d1: " << d1 << endl;
    cout << "d2: " << d2 << endl;
    
    d0 = d1;
    cout << "d0 = d1: " << d0 << endl;
    
    cout << "d0.year(): " << d0.year() << endl;
    cout << "d0.month(): " << d0.month() << endl;
    cout << "d0.day(): " << d0.day() << endl;
    cout << "d1.daysTo(d2): " << d1.daysTo(d2) << endl;
    
    return 0;
}
