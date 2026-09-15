//
//  MTLAllocation.h
//  Metal
//
//  Copyright (c) 2024 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>


/// Base class for Metal allocations.
///
/// This protocol provides a common interface for adding Metal resources to ``MTLResidencySet`` instances. Call
/// ``MTLResidencySet/addAllocation:`` to add a Metal resource allocation to a residency set.
///
/// <doc:simplifying-gpu-resource-management-with-residency-sets.md>
API_AVAILABLE(macos(15.0), ios(18.0))
@protocol MTLAllocation <NSObject>
@property (readonly) NSUInteger allocatedSize API_AVAILABLE(macos(15.0), ios(18.0));
@end

