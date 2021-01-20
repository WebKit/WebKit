/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WKContactPicker.h"

#if HAVE(CONTACTSUI)

#import "ContactsUISPI.h"
#import <Contacts/Contacts.h>
#import <WebCore/ContactInfo.h>
#import <WebCore/ContactsRequestData.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

SOFT_LINK_FRAMEWORK(Contacts)
SOFT_LINK_CLASS(Contacts, CNContactFormatter)
SOFT_LINK_CLASS(Contacts, CNLabeledValue)
SOFT_LINK_CLASS(Contacts, CNMutableContact)
SOFT_LINK_CLASS(Contacts, CNPhoneNumber)

#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)
SOFT_LINK_FRAMEWORK(ContactsUI)
SOFT_LINK_CLASS(ContactsUI, CNContactPickerViewController)
#endif

#pragma mark - Delegate Wrappers

@interface WKCNContactPickerDelegate : NSObject<CNContactPickerDelegate> {
@protected
    WeakObjCPtr<id<CNContactPickerDelegate>> _contactPickerDelegate;
}

- (instancetype)initWithContactPickerDelegate:(id<CNContactPickerDelegate>)contactPickerDelegate;

@end

@implementation WKCNContactPickerDelegate

- (instancetype)initWithContactPickerDelegate:(id<CNContactPickerDelegate>)contactPickerDelegate
{
    if (!(self = [super init]))
        return nil;

    _contactPickerDelegate = contactPickerDelegate;

    return self;
}

#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)
- (void)contactPickerDidCancel:(CNContactPickerViewController *)picker
{
    [_contactPickerDelegate contactPickerDidCancel:picker];
}
#endif

@end

@interface WKCNContactPickerSingleSelectDelegate : WKCNContactPickerDelegate
@end

@implementation WKCNContactPickerSingleSelectDelegate

#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)
- (void)contactPicker:(CNContactPickerViewController *)picker didSelectContact:(CNContact *)contact
{
    [_contactPickerDelegate contactPicker:picker didSelectContact:contact];
}
#endif

@end

@interface WKCNContactPickerMultiSelectDelegate : WKCNContactPickerDelegate
@end

@implementation WKCNContactPickerMultiSelectDelegate

#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)
- (void)contactPicker:(CNContactPickerViewController *)picker didSelectContacts:(NSArray<CNContact*> *)contacts
{
    [_contactPickerDelegate contactPicker:picker didSelectContacts:contacts];
}
#endif

@end

#pragma mark - WKContactPicker

@interface WKContactPicker () <CNContactPickerDelegate>
@end

@implementation WKContactPicker {
    WeakObjCPtr<WKWebView> _webView;
    WeakObjCPtr<id<WKContactPickerDelegate>> _delegate;

    Vector<WebCore::ContactProperty> _properties;
    WTF::CompletionHandler<void(Optional<Vector<WebCore::ContactInfo>>&&)> _completionHandler;

    RetainPtr<WKCNContactPickerDelegate> _contactPickerDelegate;
#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)
    RetainPtr<CNContactPickerViewController> _contactPickerViewController;
#endif
}

- (id<WKContactPickerDelegate>)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id<WKContactPickerDelegate>)delegate
{
    _delegate = delegate;
}

- (instancetype)initWithView:(WKWebView *)view
{
    if (!(self = [super init]))
        return nil;

    _webView = view;

    return self;
}

- (void)presentWithRequestData:(const WebCore::ContactsRequestData&)requestData completionHandler:(WTF::CompletionHandler<void(Optional<Vector<WebCore::ContactInfo>>&&)>&&)completionHandler
{
    _properties = requestData.properties;
    _completionHandler = WTFMove(completionHandler);

    if (requestData.multiple)
        _contactPickerDelegate = adoptNS([[WKCNContactPickerMultiSelectDelegate alloc] initWithContactPickerDelegate:self]);
    else
        _contactPickerDelegate = adoptNS([[WKCNContactPickerSingleSelectDelegate alloc] initWithContactPickerDelegate:self]);

#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)
    _contactPickerViewController = adoptNS([allocCNContactPickerViewControllerInstance() init]);
    [_contactPickerViewController setDelegate:_contactPickerDelegate.get()];
    [_contactPickerViewController setPrompt:requestData.url];

    UIViewController *presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:_webView.get().get()];
    [presentationViewController presentViewController:_contactPickerViewController.get() animated:YES completion:[weakSelf = WeakObjCPtr<WKContactPicker>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        if ([[strongSelf delegate] respondsToSelector:@selector(contactPickerDidPresent:)])
            [[strongSelf delegate] contactPickerDidPresent:strongSelf.get()];
    }];
#endif
}

#pragma mark - Completion

#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)

- (void)contactPickerDidCancel:(CNContactPickerViewController *)picker
{
    Vector<WebCore::ContactInfo> info;
    [self _contactPickerDidDismissWithContactInfo:WTFMove(info)];
}

- (void)contactPicker:(CNContactPickerViewController *)picker didSelectContact:(CNContact *)contact
{
    Vector<WebCore::ContactInfo> info = { [self _contactInfoFromCNContact:contact] };
    [self _contactPickerDidDismissWithContactInfo:WTFMove(info)];
}

- (void)contactPicker:(CNContactPickerViewController *)picker didSelectContacts:(NSArray<CNContact*> *)contacts
{
    Vector<WebCore::ContactInfo> info;
    info.reserveInitialCapacity(contacts.count);
    for (CNContact *contact in contacts)
        info.uncheckedAppend([self _contactInfoFromCNContact:contact]);
    [self _contactPickerDidDismissWithContactInfo:WTFMove(info)];
}

#endif

- (void)_contactPickerDidDismissWithContactInfo:(Vector<WebCore::ContactInfo>&&)info
{
    _completionHandler(WTFMove(info));

    if ([_delegate respondsToSelector:@selector(contactPickerDidDismiss:)])
        [_delegate contactPickerDidDismiss:self];
}

- (WebCore::ContactInfo)_contactInfoFromCNContact:(CNContact *)contact
{
    WebCore::ContactInfo contactInfo;

    if (_properties.contains(WebCore::ContactProperty::Name)) {
        NSString *contactName = [getCNContactFormatterClass() stringFromContact:contact style:CNContactFormatterStyleFullName];
        contactInfo.name = { contactName };
    }

    if (_properties.contains(WebCore::ContactProperty::Email)) {
        for (CNLabeledValue<NSString *> *emailAddress in contact.emailAddresses)
            contactInfo.email.append(emailAddress.value);
    }

    if (_properties.contains(WebCore::ContactProperty::Tel)) {
        for (CNLabeledValue<CNPhoneNumber *> *phoneNumber in contact.phoneNumbers)
            contactInfo.tel.append(phoneNumber.value.stringValue);
    }

    return contactInfo;
}

#pragma mark - Testing

- (void)dismissWithContacts:(NSArray *)contacts
{
#if HAVE(CNCONTACTPICKERVIEWCONTROLLER)
    [_contactPickerViewController dismissViewControllerAnimated:NO completion:[self, weakSelf = WeakObjCPtr<WKContactPicker>(self), jsContacts = RetainPtr<NSArray>(contacts)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        [strongSelf contactPicker:_contactPickerViewController.get() didSelectContacts:[strongSelf _contactsFromJSContacts:jsContacts.get()]];
    }];
#endif
}

- (NSArray<CNContact*> *)_contactsFromJSContacts:(NSArray *)jsContacts
{
    if (!jsContacts)
        return [NSArray array];

    NSMutableArray<CNContact*> *contacts = [NSMutableArray arrayWithCapacity:jsContacts.count];

    NSPredicate *stringValuePredicate = [NSPredicate predicateWithFormat:@"self isKindOfClass: %@", [NSString class]];

    for (id jsContact in jsContacts) {
        if (![jsContact isKindOfClass:[NSDictionary class]])
            continue;

        CNMutableContact *contact = [[allocCNMutableContactInstance() init] autorelease];

        id names = [(NSDictionary *)jsContact objectForKey:@"name"];
        if ([names isKindOfClass:[NSArray class]]) {
            for (NSString *name in [names filteredArrayUsingPredicate:stringValuePredicate]) {
                contact.givenName = name;
                break;
            }
        }

        id emails = [(NSDictionary *)jsContact objectForKey:@"email"];
        if ([emails isKindOfClass:[NSArray class]]) {
            NSMutableArray<CNLabeledValue<NSString*>*> *emailAddresses = [NSMutableArray array];
            for (NSString *email in [emails filteredArrayUsingPredicate:stringValuePredicate]) {
                CNLabeledValue<NSString*> *labeledValue = [getCNLabeledValueClass() labeledValueWithLabel:nil value:email];
                [emailAddresses addObject:labeledValue];
            }
            contact.emailAddresses = emailAddresses;
        }

        id phoneNumbers = [(NSDictionary *)jsContact objectForKey:@"tel"];
        if ([phoneNumbers isKindOfClass:[NSArray class]]) {
            NSMutableArray<CNLabeledValue<CNPhoneNumber*>*> *numbers = [NSMutableArray array];
            for (NSString *phoneNumber in [phoneNumbers filteredArrayUsingPredicate:stringValuePredicate]) {
                CNPhoneNumber *cnPhoneNumber = [getCNPhoneNumberClass() phoneNumberWithStringValue:phoneNumber];
                CNLabeledValue<CNPhoneNumber*> *labeledValue = [getCNLabeledValueClass() labeledValueWithLabel:nil value:cnPhoneNumber];
                [numbers addObject:labeledValue];
            }
            contact.phoneNumbers = numbers;
        }

        [contacts addObject:contact];
    }

    return contacts;
}

@end

#endif // HAVE(CONTACTSUI)
