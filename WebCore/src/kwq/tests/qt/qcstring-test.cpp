#include <iostream>

#include <qcstring.h>

int main() {

    QCString s1 = "this is a string";
    QCString s2 = "this is another string";
    QCString s3 = "jazz hype loves two mugs for dire quixotic knobs";
    QCString s4; 
    QCString s5 = s1; 

    cout << s1 << endl;
    cout << s2 << endl;
    cout << s3 << endl;
    cout << s4 << endl;
    cout << s5 << endl;

    QCString s6 = s1.upper();
    cout << "upper case: " << s6 << endl;
    cout << "lower case: " << s6.lower() << endl;

    cout << "left four chars: " << s3.left(4) << endl;
    cout << "right five chars: " << s3.right(5) << endl;

    s6 = s3;
    s6.truncate(15);
    cout << s6 << endl;

    s6 += " two mugs for dire quixotic knobs";
    cout << s6 << endl;
    cout << "should be non-zero: " << (s3 == s6) << endl;

    return 0;
}
