#ifndef QINTDICT_H_
#define QINTDICT_H_

#include "KWQDef.h"
#include "KWQCollection.h"

#include "KWQPtrDictImpl.h"

#include <qptrcollection.h>

template <class T> class QIntDictIterator;

template <class T> class QIntDict : public QPtrCollection{
public:
    QIntDict(int size=17) : impl(size, deleteFunc) {}
    virtual ~QIntDict() { impl.clear(del_item); }

    virtual void clear() { impl.clear(del_item); }
    virtual uint count() const { return impl.count(); }

    T *take(int key) { return (T *)impl.take((void *)key); }
    void insert(int key, T *value) { impl.insert((void *)key, value); }
    bool remove(int key) { return impl.remove((void *)key, del_item); }
    void replace(int key, T *value) {
      if (find((void *)key)) remove((void *)key);
      insert((void *)key, value);
    }
    T* find(int key) { return (T*)impl.find((void *)key); }

    bool isEmpty() const { return count() == 0; }

    QIntDict<T> &operator=(const QIntDict<T> &pd) { impl.assign(pd.impl,del_item); QPtrCollection::operator=(pd); return *this; }
    T *operator[](int key) const { return (T *)impl.find((void *)key); } 

 private:
    static void deleteFunc(void *item) { delete (T *)item; }

    KWQPtrDictImpl impl;
    
    friend class QIntDictIterator<T>;
};

template <class T> class QIntDictIterator {
public:
    QIntDictIterator(const QIntDict<T> &pd) : impl(pd.impl) { }
    uint count() { return impl.count(); }
    T *current() const { return (T *)impl.current(); }
    int currentKey() const { return (int)impl.currentKey(); }

    T* toFirst() { return (T*)(impl.toFirst()); }

    T *operator++() { return (T *)++impl; }

private:
    KWQPtrDictIteratorImpl impl;

    QIntDictIterator(const QIntDictIterator &);
    QIntDictIterator &operator=(const QIntDictIterator &);
};

#define Q3IntDict QIntDict
#define Q3IntDictIterator QIntDictIterator

#endif

