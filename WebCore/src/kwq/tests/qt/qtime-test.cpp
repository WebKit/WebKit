#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qdatetime.h>

int main() {

    QTime d0;
    QTime d1(23, 59);
    QTime d2(1, 8);
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
    cout << "d2 hour: " << d2.hour() << endl;
    cout << "d2 minute: " << d2.minute() << endl;
    cout << "d2 second: " << d2.second() << endl;
    
    cout << "d2 secsTo d1: " << d2.secsTo(d1) << endl;
    
    elapsedTime = d3.elapsed(); // nothing is printed because output will never match .chk file
    elapsedTime = d3.restart();
    
    
    
    return 0;
}
