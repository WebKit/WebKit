#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qdatetime.h>

int main() {

    QDate d1(1999, 12, 31);
    QDate d2(2000, 12, 31);
    
    QTime t1(23, 59);
    QTime t2(23, 59);
    
    QDateTime dt0;       
    QDateTime dt2(d1, t1);    
    QDateTime dt3(d2, t2);
    QDateTime dt4(dt3);

    cout << "dt2: " << dt2 << endl;
    cout << "dt3: " << dt3 << endl;
    cout << "dt4: " << dt4 << endl;
    
    dt0 = dt2;
    cout << "dt0=dt2: " << dt0 << endl;
    
    cout << "dt2 secsTo dt3: " << dt2.secsTo(dt3) << endl;
    cout << "dt2 time: " << dt2.time() << endl;
    


    return 0;
}
