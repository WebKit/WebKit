#import <Cocoa/Cocoa.h>


class QWidget;

@interface KWQView : NSView
{
    QWidget *widget;
}
- initWithFrame: (NSRect)r widget: (QWidget *)w; 
@end

@interface KWQNSButton : NSButton
{
    QWidget *widget;
}
- initWithFrame: (NSRect)r widget: (QWidget *)w; 
@end
