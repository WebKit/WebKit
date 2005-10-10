//
// JSBase.h
//

#ifndef JSBase_h
#define JSBase_h

#include "JSUtils.h"

class JSBase {
    public:
        JSBase(JSTypeID type);
        virtual ~JSBase();

        JSBase* Retain();
        void Release();
        CFIndex RetainCount() const;
        JSTypeID GetTypeID() const;

        virtual CFStringRef CopyDescription();
        virtual UInt8 Equal(JSBase* other);

    private:
        JSTypeID fTypeID;
        CFIndex fRetainCount;
};

#endif
