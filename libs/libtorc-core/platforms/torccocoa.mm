// OS X
#import <Cocoa/Cocoa.h>

// Torc
#include "torccocoa.h"

void* CreateOSXCocoaPool(void);
void  DeleteOSXCocoaPool(void *&pool);

// Dummy NSThread for Cocoa multithread initialization
@implementation NSThread (Dummy)

- (void) run;
{
}

@end

void *CreateOSXCocoaPool(void)
{
    // Cocoa requires a message to be sent informing the Cocoa event
    // thread that the application is multi-threaded. Apple recommends
    // creating a dummy NSThread to get this message sent.

    if (![NSThread isMultiThreaded])
    {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSThread *thr = [[NSThread alloc] init];
        SEL threadSelector = @selector(run);
        [NSThread detachNewThreadSelector:threadSelector toTarget:thr withObject:nil];
        [pool release];
    }

    NSAutoreleasePool *pool = NULL;
    pool = [[NSAutoreleasePool alloc] init];
    return pool;
}

void DeleteOSXCocoaPool(void* &pool)
{
    if (pool)
    {
        NSAutoreleasePool *a_pool = (NSAutoreleasePool*) pool;
        pool = NULL;
        [a_pool release];
    }
}

CocoaAutoReleasePool::CocoaAutoReleasePool()
{
    m_pool = CreateOSXCocoaPool();
}

CocoaAutoReleasePool::~CocoaAutoReleasePool()
{
    if (m_pool)
        DeleteOSXCocoaPool(m_pool);
}

CGDirectDisplayID GetOSXCocoaDisplay(void* Window)
{
    NSView *thisview = static_cast<NSView *>(Window);
    if (!thisview)
        return NULL;
    NSScreen *screen = [[thisview window] screen];
    if (!screen)
        return NULL;
    NSDictionary* desc = [screen deviceDescription];
    return (CGDirectDisplayID)[[desc objectForKey:@"NSScreenNumber"] intValue];
}
