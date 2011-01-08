#ifndef DataGridColumnList_h
#define DataGridColumnList_h

#if ENABLE(DATAGRID)

#include "DataGridColumn.h"

#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTMLDataGridElement;

class DataGridColumnList : public RefCounted<DataGridColumnList> {
    friend class DataGridColumn;
public:
    static PassRefPtr<DataGridColumnList> create(HTMLDataGridElement* grid)
    {
        return adoptRef(new DataGridColumnList(grid));
    }

    ~DataGridColumnList();

    unsigned length() const { return m_columns.size(); }

    DataGridColumn* item(unsigned index) const { return m_columns[index].get(); }
    DataGridColumn* itemWithName(const AtomicString&) const;

    DataGridColumn* primaryColumn() const { return m_primaryColumn.get(); }

    DataGridColumn* sortColumn() const { return m_sortColumn.get(); }

    DataGridColumn* add(const String& id, const String& label, const String& type, bool primary, unsigned short sortable);
    DataGridColumn* add(DataGridColumn*);
    void remove(DataGridColumn*);
    void move(DataGridColumn*, unsigned long index);
    void clear();

    HTMLDataGridElement* dataGrid() const { return m_dataGrid; }
    void clearDataGrid() { m_dataGrid = 0; }

    void setDataGridNeedsLayout();

private:
    DataGridColumnList(HTMLDataGridElement*);

    void primaryColumnChanged(DataGridColumn*);

    HTMLDataGridElement* m_dataGrid; // Weak reference.  Will be nulled out when our tree goes away.

    Vector<RefPtr<DataGridColumn> > m_columns;
    RefPtr<DataGridColumn> m_primaryColumn;
    RefPtr<DataGridColumn> m_sortColumn;
};

} // namespace WebCore

#endif

#endif // DataGridColumnList_h
