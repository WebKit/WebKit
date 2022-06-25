/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDUtilities.h"

#import <mach/mach.h>

#import "sdk/objc/base/RTCLogging.h"

@implementation NSDictionary (ARDUtilites)

+ (NSDictionary *)dictionaryWithJSONString:(NSString *)jsonString {
  NSParameterAssert(jsonString.length > 0);
  NSData *data = [jsonString dataUsingEncoding:NSUTF8StringEncoding];
  NSError *error = nil;
  NSDictionary *dict =
      [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
  if (error) {
    RTCLogError(@"Error parsing JSON: %@", error.localizedDescription);
  }
  return dict;
}

+ (NSDictionary *)dictionaryWithJSONData:(NSData *)jsonData {
  NSError *error = nil;
  NSDictionary *dict =
      [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&error];
  if (error) {
    RTCLogError(@"Error parsing JSON: %@", error.localizedDescription);
  }
  return dict;
}

@end

@implementation NSURLConnection (ARDUtilities)

+ (void)sendAsyncRequest:(NSURLRequest *)request
       completionHandler:(void (^)(NSURLResponse *response,
                                   NSData *data,
                                   NSError *error))completionHandler {
  // Kick off an async request which will call back on main thread.
  NSURLSession *session = [NSURLSession sharedSession];
  [[session dataTaskWithRequest:request
              completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                if (completionHandler) {
                  completionHandler(response, data, error);
                }
              }] resume];
}

// Posts data to the specified URL.
+ (void)sendAsyncPostToURL:(NSURL *)url
                  withData:(NSData *)data
         completionHandler:(void (^)(BOOL succeeded,
                                     NSData *data))completionHandler {
  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
  request.HTTPMethod = @"POST";
  request.HTTPBody = data;
  [[self class] sendAsyncRequest:request
                completionHandler:^(NSURLResponse *response,
                                    NSData *data,
                                    NSError *error) {
    if (error) {
      RTCLogError(@"Error posting data: %@", error.localizedDescription);
      if (completionHandler) {
        completionHandler(NO, data);
      }
      return;
    }
    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
    if (httpResponse.statusCode != 200) {
      NSString *serverResponse = data.length > 0 ?
          [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] :
          nil;
      RTCLogError(@"Received bad response: %@", serverResponse);
      if (completionHandler) {
        completionHandler(NO, data);
      }
      return;
    }
    if (completionHandler) {
      completionHandler(YES, data);
    }
  }];
}

@end

NSInteger ARDGetCpuUsagePercentage() {
  // Create an array of thread ports for the current task.
  const task_t task = mach_task_self();
  thread_act_array_t thread_array;
  mach_msg_type_number_t thread_count;
  if (task_threads(task, &thread_array, &thread_count) != KERN_SUCCESS) {
    return -1;
  }

  // Sum cpu usage from all threads.
  float cpu_usage_percentage = 0;
  thread_basic_info_data_t thread_info_data = {};
  mach_msg_type_number_t thread_info_count;
  for (size_t i = 0; i < thread_count; ++i) {
    thread_info_count = THREAD_BASIC_INFO_COUNT;
    kern_return_t ret = thread_info(thread_array[i],
                                    THREAD_BASIC_INFO,
                                    (thread_info_t)&thread_info_data,
                                    &thread_info_count);
    if (ret == KERN_SUCCESS) {
      cpu_usage_percentage +=
          100.f * (float)thread_info_data.cpu_usage / TH_USAGE_SCALE;
    }
  }

  // Dealloc the created array.
  vm_deallocate(task, (vm_address_t)thread_array,
                sizeof(thread_act_t) * thread_count);
  return lroundf(cpu_usage_percentage);
}
