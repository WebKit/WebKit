#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qdatetime.h>

int main() {

    QTime d0;

    QTime d1 = QTime(23, 59, 59, 999);
    QTime d2 = QTime(1, 8, 6, 0);
    QTime d3;
    int elapsedTime;
    
    d3.start();
    cout << "d1: " << d1 << endl;
    cout << "d2: " << d2 << endl;
    
    cout << "d0 isNull: " << d0.isNull() << endl;
    cout << "d1 isNull: " << d1.isNull() << endl;
    
    d0 = d1;
    cout << "d0 = d1: " << d0 << endl;
    
    cout << "d1 msec: " << d1.msec() << endl;
    cout << "d2 msec: " << d2.msec() << endl;
    
    
    elapsedTime = d3.elapsed(); // nothing is printed because output will never match .chk file
    elapsedTime = d3.restart();
    
    
    //d0 = d1.addSecs(1);
    //cout << "day roll (seconds): " << d0 << endl;

    //d0 = d1;
    //d0 = d1.addMSecs(1);
    //cout << "day roll (milli-seconds): " << d0 << endl;

    //d2 = d1;
    //cout << "d0 is: " << d0 << endl;
    //cout << "d1 is: " << d1 << endl;
    //cout << "d2 is: " << d2 << endl;
    //cout << "(d0 < d1): " << (d0 < d1) << endl;
    //cout << "(d0 <= d1): " << (d0 <= d1) << endl;
    //cout << "(d0 > d1): " << (d0 > d1) << endl;
    //cout << "(d0 >= d1): " << (d0 >= d1) << endl;
    //cout << "(d2 < d1): " << (d2 < d1) << endl;
    //cout << "(d2 <= d1): " << (d2 <= d1) << endl;
    //cout << "(d2 > d1): " << (d2 > d1) << endl;
    //cout << "(d2 >= d1): " << (d2 >= d1) << endl;
    
    return 0;
}
