#include <iostream>

#include <qmap.h>

int main() {

    QMap<int,int> m1;

    m1.insert(1,10);
    m1.insert(2,20);

    cout << m1 << endl;

    return 0;
}
