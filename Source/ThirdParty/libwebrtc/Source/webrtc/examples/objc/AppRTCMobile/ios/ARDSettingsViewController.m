/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSettingsViewController.h"
#import "ARDSettingsModel.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(int, ARDSettingsSections) {
  ARDSettingsSectionMediaConstraints = 0,
  ARDSettingsSectionBitRate
};

@interface ARDSettingsViewController () <UITextFieldDelegate> {
  ARDSettingsModel *_settingsModel;
}

@end

@implementation ARDSettingsViewController

- (instancetype)initWithStyle:(UITableViewStyle)style
                settingsModel:(ARDSettingsModel *)settingsModel {
  self = [super initWithStyle:style];
  if (self) {
    _settingsModel = settingsModel;
  }
  return self;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Settings";
  [self addDoneBarButton];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self selectCurrentlyStoredOrDefaultMediaConstraints];
}

#pragma mark - Data source

- (NSArray<NSString *> *)mediaConstraintsArray {
  return _settingsModel.availableVideoResoultionsMediaConstraints;
}

#pragma mark -

- (void)addDoneBarButton {
  UIBarButtonItem *barItem =
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                    target:self
                                                    action:@selector(dismissModally:)];
  self.navigationItem.leftBarButtonItem = barItem;
}

- (void)selectCurrentlyStoredOrDefaultMediaConstraints {
  NSString *currentSelection = [_settingsModel currentVideoResoultionConstraintFromStore];

  NSUInteger indexOfSelection = [[self mediaConstraintsArray] indexOfObject:currentSelection];
  NSIndexPath *pathToBeSelected = [NSIndexPath indexPathForRow:indexOfSelection inSection:0];
  [self.tableView selectRowAtIndexPath:pathToBeSelected
                              animated:NO
                        scrollPosition:UITableViewScrollPositionNone];
  // Manully invoke the delegate method because the previous invocation will not.
  [self tableView:self.tableView didSelectRowAtIndexPath:pathToBeSelected];
}

#pragma mark - Dismissal of view controller

- (void)dismissModally:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  if ([self sectionIsMediaConstraints:section]) {
    return self.mediaConstraintsArray.count;
  }

  return 1;
}

#pragma mark - Index path helpers

- (BOOL)sectionIsMediaConstraints:(int)section {
  return section == ARDSettingsSectionMediaConstraints;
}

- (BOOL)sectionIsBitrate:(int)section {
  return section == ARDSettingsSectionBitRate;
}

- (BOOL)indexPathIsMediaConstraints:(NSIndexPath *)indexPath {
  return [self sectionIsMediaConstraints:indexPath.section];
}

- (BOOL)indexPathIsBitrate:(NSIndexPath *)indexPath {
  return [self sectionIsBitrate:indexPath.section];
}

#pragma mark - Table view delegate

- (nullable NSString *)tableView:(UITableView *)tableView
         titleForHeaderInSection:(NSInteger)section {
  if ([self sectionIsMediaConstraints:section]) {
    return @"Media constraints";
  }

  if ([self sectionIsBitrate:section]) {
    return @"Maximum bitrate";
  }

  return @"";
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  if ([self indexPathIsMediaConstraints:indexPath]) {
    return [self mediaConstraintsTableViewCellForTableView:tableView atIndexPath:indexPath];
  }

  if ([self indexPathIsBitrate:indexPath]) {
    return [self bitrateTableViewCellForTableView:tableView atIndexPath:indexPath];
  }

  return [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                reuseIdentifier:@"identifier"];
}

- (nullable NSIndexPath *)tableView:(UITableView *)tableView
           willSelectRowAtIndexPath:(nonnull NSIndexPath *)indexPath {
  if ([self indexPathIsMediaConstraints:indexPath]) {
    return [self tableView:tableView willDeselectMediaConstraintsRowAtIndexPath:indexPath];
  }
  return indexPath;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  if ([self indexPathIsMediaConstraints:indexPath]) {
    [self tableView:tableView didSelectMediaConstraintsCellAtIndexPath:indexPath];
  }
}

#pragma mark - Table view delegate(Media Constraints)

- (UITableViewCell *)mediaConstraintsTableViewCellForTableView:(UITableView *)tableView
                                                   atIndexPath:(NSIndexPath *)indexPath {
  NSString *dequeueIdentifier = @"ARDSettingsMediaConstraintsViewCellIdentifier";
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:dequeueIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:dequeueIdentifier];
  }
  cell.textLabel.text = self.mediaConstraintsArray[indexPath.row];
  return cell;
}

- (void)tableView:(UITableView *)tableView
    didSelectMediaConstraintsCellAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;

  NSString *mediaConstraintsString = self.mediaConstraintsArray[indexPath.row];
  [_settingsModel storeVideoResoultionConstraint:mediaConstraintsString];
}

- (NSIndexPath *)tableView:(UITableView *)tableView
    willDeselectMediaConstraintsRowAtIndexPath:(NSIndexPath *)indexPath {
  NSIndexPath *oldSelection = [tableView indexPathForSelectedRow];
  UITableViewCell *cell = [tableView cellForRowAtIndexPath:oldSelection];
  cell.accessoryType = UITableViewCellAccessoryNone;
  return indexPath;
}

#pragma mark - Table view delegate(Bitrate)

- (UITableViewCell *)bitrateTableViewCellForTableView:(UITableView *)tableView
                                          atIndexPath:(NSIndexPath *)indexPath {
  NSString *dequeueIdentifier = @"ARDSettingsBitrateCellIdentifier";
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:dequeueIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:dequeueIdentifier];

    UITextField *textField = [[UITextField alloc]
        initWithFrame:CGRectMake(10, 0, cell.bounds.size.width - 20, cell.bounds.size.height)];
    NSString *currentMaxBitrate = [_settingsModel currentMaxBitrateSettingFromStore].stringValue;
    textField.text = currentMaxBitrate;
    textField.placeholder = @"Enter max bit rate (kbps)";
    textField.keyboardType = UIKeyboardTypeNumberPad;
    textField.delegate = self;

    // Numerical keyboards have no return button, we need to add one manually.
    UIToolbar *numberToolbar =
        [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, self.view.bounds.size.width, 50)];
    numberToolbar.items = @[
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                                                    target:nil
                                                    action:nil],
      [[UIBarButtonItem alloc] initWithTitle:@"Apply"
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(numberTextFieldDidEndEditing:)]
    ];
    [numberToolbar sizeToFit];

    textField.inputAccessoryView = numberToolbar;
    [cell addSubview:textField];
  }
  return cell;
}

- (void)numberTextFieldDidEndEditing:(id)sender {
  [self.view endEditing:YES];
}

- (void)textFieldDidEndEditing:(UITextField *)textField {
  NSNumber *bitrateNumber = nil;

  if (textField.text.length != 0) {
    bitrateNumber = [NSNumber numberWithInteger:textField.text.intValue];
  }

  [_settingsModel storeMaxBitrateSetting:bitrateNumber];
}

@end
NS_ASSUME_NONNULL_END
