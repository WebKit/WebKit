/*	
    WebKitSystemBits.m
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebKitSystemBits.h>

#import <WebKit/WebAssertions.h>

#include <mach/mach.h>
#include <mach/host_info.h>
#include <mach/mach_error.h>

#include <sys/types.h>
#include <sys/sysctl.h>

static host_basic_info_data_t gHostBasicInfo;
static pthread_once_t initControl = PTHREAD_ONCE_INIT;

static void initCapabilities()
{
    mach_msg_type_number_t  count;
    kern_return_t r;
    mach_port_t host;

    /* Discover our CPU type */
    host = mach_host_self();
    count = HOST_BASIC_INFO_COUNT;
    r = host_info(host, HOST_BASIC_INFO, (host_info_t) &gHostBasicInfo, &count);
    mach_port_deallocate(mach_task_self(), host);
    if (r != KERN_SUCCESS) {
        ERROR("%s : host_info(%d) : %s.\n", __FUNCTION__, r, mach_error_string(r));
    }
}

vm_size_t WebSystemMainMemory(void)
{
    pthread_once(&initControl, initCapabilities);
    return gHostBasicInfo.memory_size;
}

int WebNumberOfCPUs(void)
{
    static int numCPUs = 0;
    
    if (numCPUs == 0) {
        int mib[2];
        size_t len;

        mib[0] = CTL_HW;
        mib[1] = HW_NCPU;
        len = sizeof(numCPUs);
        sysctl(mib, 2, &numCPUs, &len, NULL, 0);
    }
    return numCPUs;
}
