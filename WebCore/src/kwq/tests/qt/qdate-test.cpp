#include <iostream>

#include <qdatetime.h>

int main() {

    QDate d0;
    QDate d1 = QDate(1999, 12, 31);
    QDate d2;

    cout << d1 << endl;

    d0 = d1;
    cout << "assignment: " << d0 << endl;

    d0 = d1.addDays(1);
    cout << "y2k: " << d0 << endl;

    d1 = QDate(1900, 2, 28);
    d0 = d1.addDays(1);
    cout << "is not leap year: " << d0 << endl;

    d1 = QDate(2000, 2, 28);
    d0 = d1.addDays(1);
    cout << "is leap year: " << d0 << endl;

    cout << d1.monthName(d1.month()) << endl;

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
