#include <iostream>

#include <qdatetime.h>

int main() {

    QDate d0;
    QDate d1 = QDate(1999, 12, 31);

    QTime t0;
    QTime t1 = QTime(23, 59, 59, 999);

    QDateTime dt0;    
    QDateTime dt1 = QDateTime(d1);    
    QDateTime dt2 = QDateTime(d1, t1);    

    cout << dt1 << endl;
    cout << dt2 << endl;

    dt0 = dt1;
    cout << "assignment: " << dt0 << endl;
    cout << "string: " << dt0.toString() << endl;

    d0 = d1.addDays(1);
    dt0.setDate(d0);
    cout << "setDate: " << dt0 << endl;

    t0 = t1.addSecs(1);
    dt0.setTime(t0);
    cout << "setTime: " << dt0 << endl;

    dt0.setTime_t(1000000000);
    cout << "setTime_t: " << dt0 << endl;

    return 0;
}
