
#ifndef ScrollAnimatorBlackBerry_h
#define ScrollAnimatorBlackberry_h

#if ENABLE(SMOOTH_SCROLLING)

#include "ScrollAnimator.h"
#include "ScrollAnimatorNone.h"

namespace WebCore {

class ScrollableArea;

class ScrollAnimatorBlackBerry : public ScrollAnimatorNone {
public:
    ScrollAnimatorBlackBerry(ScrollableArea*);

    void setDisableConstrainsScrollingToContentEdgeWhileAnimating(bool);

protected:
    virtual void animationWillStart();
    virtual void animationDidFinish();

private:
    bool m_disableConstrainsScrollingToContentEdgeWhileAnimating;
    bool m_originalConstransScrollingToContentEdge;
};

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)

#endif // ScrollAnimatorBlackBerry_h
