# Queue Present Wait Semaphore Management

The following shorthand notations are used throughout this document:

- PE: Presentation Engine
- ANI: vkAcquireNextImageKHR
- QS: vkQueueSubmit
- QP: vkQueuePresentKHR
- W: Wait
- S: Signal
- R: Render
- P: Present
- SN: Semaphore N
- IN: Swapchain image N
- FN: Fence N

---

## Introduction

Vulkan requires the application (ANGLE in this case) to acquire swapchain images and queue them for
presentation, synchronizing GPU submissions with semaphores.  A single frame looks like the
following:

    CPU: ANI  ... QS   ... QP
         S:S1     W:S1     W:S2
                  S:S2
    GPU:          <------------ R ----------->
     PE:                                      <-------- P ------>

That is, the GPU starts rendering after submission, and the presentation is started when rendering is
finished.  Note that Vulkan tries to abstract a large variety of PE architectures, some of which do
not behave in a straight-forward manner.  As such, ANGLE cannot know what the PE is exactly doing
with the images or when the images are visible on the screen.  The only signal out of the PE is
received through the semaphore that's used in ANI.

With multiple frames, the pipeline looks different based on present mode.  Let's focus on
FIFO (the arguments in this document translate to all modes) with 3 images:

    CPU: QS QP QS QP QS QP QS QP
         I1 I1 I2 I2 I3 I3 I1 I1
    GPU: <---- R I1 ----><---- R I2 ----><---- R I3 ----><---- R I1 ---->
     PE:                 <----- P I1 -----><----- P I2 -----><----- P I3 -----><----- P I1 ----->

First, an issue is evident here.  The CPU is submitting jobs and queuing images for presentation
faster than the GPU can render them or the PE can view them.  This can cause the length of the
submit queue to grow indefinitely, resulting in larger and larger input lag.  In FIFO mode, the PE
present queue also grows indefinitely.

To address this issue, ANGLE paces the CPU such that the length of the submit queue is kept at a
maximum of 1 image (i.e. submission with one image is being processed, and another one is in queue):

    CPU: QS   QS          W:F1 QS         W:F2 QS
         I1   I2               I3              I1
         S:F1 S:F2             S:F3            S:F4
    GPU: <---- R I1 ----><---- R I2 ----><---- R I3 ----><---- R I1 ---->

> Note: Ideally, the length of the PE present queue should also be kept at a maximum of 1 (i.e. one
> image being presented, and another in queue).  However, the Vulkan WSI extension doesn't provide
> enough control to achieve this.  In heavy application, the length of the PE present queue is
> probably 1 anyway (as the rendering time is almost as long as the frame (i.e. present time), in
> which case pacing the submissions similarly paces the presentation).  In theory, in FIFO mode, the
> length of the PE present queue is below n+2 where n is the number of swapchain images.
>
> To understand why, imagine a FIFO swapchain with 1000 images and submissions that are
> infinitesimally short.  In this case, the CPU pacing is effectively a no-op (as the GPU instantly
> finishes jobs) for the first 1002 submissions.  The 1003rd submission waits for F1001 (which uses
> I1).  However, the 1001st submission will not start until the PE switches to presenting I2 (at the
> next V-Sync).  The CPU then waits for V-Sync before the 1003rd submission.  The CPU waits for one
> V-Sync for every subsequent submission, keeping the length of the queue 1002.
> [`VK_GOOGLE_display_timing`][DisplayTimingGOOGLE] is likely a solution to this problem.

Associated with each QP operation is a semaphore signaled by the preceding QS and waited on by the
PE before the image can be presented.  Currently, there's no feedback from Vulkan (See [internal
Khronos issue][VulkanIssue1060]) regarding _when_ the PE has actually finished waiting on the
semaphore!  This means that the application cannot generally know when to destroy the corresponding
semaphore.  However, taking ANGLE's CPU pacing into account, we are able to destroy (or rather
reuse) semaphores when they are provably unused.

This document describes an approach for destroying semaphores that should work with all valid PE
architectures, but will be described in terms of more common PE architectures (e.g. where the PE
only backs each VkImage and VkSemaphore handle with one actual memory object, and where the PE
cycles between the swapchain images in a straight-forward manner).

The interested reader may follow the discussion in this abandoned [gerrit CL][CL1757018] for more
background and ideas.

[DisplayTimingGOOGLE]: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_GOOGLE_display_timing.html
[VulkanIssue1060]: https://gitlab.khronos.org/vulkan/vulkan/issues/1060
[CL1757018]: https://chromium-review.googlesource.com/c/angle/angle/+/1757018

## Determining When a QP Semaphore is Waited On

Let's combine the above diagrams with all the details:

    CPU: ANI   | QS    | QP    | ANI   | QS    | QP    | ANI   | W:F1 | QS    | QP    | ANI   | W:F2 | QS    | QP
         I1    | I1    | I1    | I2    | I2    | I2    | I3    |      | I3    | I3    | I1    |      | I1    | I1
         S:SA1 | W:SA1 |       | S:SA2 | W:SA2 |       | S:SA3 |      | W:SA3 |       | S:SA4 |      | W:SA4 |
               | S:SP1 | W:SP1 |       | S:SP2 | W:SP2 |       |      | S:SP3 | W:SP3 |       |      | S:SP4 | W:SP4
               | S:F1  |       |       | S:F2  |       |       |      | S:F3  |       |       |      | S:F4  |

Let's focus only on sequences that return the same image:

    CPU: ANI   | W:F(X-2) | QS    | QP    | ... | ANI   | W:F(Y-2) | QS    | QP
         I1    |          | I1    | I1    |     | I1    |          | I1    | I1
         S:SAX |          | W:SAX |       |     | S:SAY |          | W:SAY |
               |          | S:SPX | W:SPX |     |       |          | S:SPY | W:SPY
               |          | S:FX  |       |     |       |          | S:FY  |

Note that X and Y are arbitrarily distanced (including possibly being sequential).

Say we are at frame Y+2.  There's therefore a wait on FY.  The following holds:

    FY is signaled
    => SAY is signaled
    => The PE has handed I1 back to the application
    => The PE has already processed the *previous* QP of I1
    => SPX is waited on

At this point, we can destroy SPX.  In other words, in frame Y+2, we can destroy SPX (note that 2 is
the number of frames the CPU pacing code uses).  If frame Y+1 is not using I1, this means the
history of present semaphores for I1 would be `{SPX, SPY}` and we can destroy the oldest semaphore
in this list.  If frame Y+1 is also using I1, we should still destroy SPX in frame Y+2, but the
history of the present semaphores for I1 would be `{SPX, SPY, SP(Y+1)}`.

In the Vulkan backend, we simplify destruction of semaphores by always keeping a history of 3
present semaphores for each image (again, 3 is H+1 where H is the swap history size used in CPU
pacing) and always reuse (instead of destroy) the oldest semaphore of the image that is about to be
presented.

To summarize, we use the completion of a submission using an image to prove when the semaphore used
for the *previous* presentation of that image is no longer in use (and can be safely destroyed or
reused).

## Swapchain recreation

When recreating the swapchain, all images are eventually freed and new ones are created, possibly
with a different count and present mode.  For the old swapchain, we can no longer rely on the
completion of a future submission to know when a previous presentation's semaphore can be destroyed,
as there won't be any more submissions using images from the old swapchain.

> For example, imagine the old swapchain was created in FIFO mode, and one image is being presented
> until the next V-Sync.  Furthermore, imagine the new swapchain is created in MAILBOX mode.  Since
> the old swapchain's image will remain presented until V-Sync, the new MAILBOX swapchain can
> perform an arbitrarily large number of (throw-away) presentations.  The old swapchain (and its
> associated present semaphores) cannot be destroyed until V-Sync; a signal that's not captured by
> Vulkan.

ANGLE resolves this issue by deferring the destruction of the old swapchain and its remaining
present semaphores to the time when the semaphore corresponding to the first present of the new
swapchain can be destroyed.  In the example in the previous section, if SPX is the present semaphore
of the first QP performed on the new swapchain, at frame Y+2, when we know SPX can be destroyed, we
know that the first image of the new swapchain has already been presented.  This proves that all
previous QPs of the old swapchain have been processed.

> Note: the swapchain can potentially be destroyed much earlier, but with no feedback from the
> presentation engine, we cannot know that.  This delays means that the swapchain could be recreated
> while there are pending old swapchains to be destroyed.  The destruction of both old swapchains
> must now be deferred to when the first QP of the new swapchain has been processed.  If an
> application resizes the window constantly and at a high rate, ANGLE would keep accumulating old
> swapchains and not free them until it stops.  While a user will likely not be able to do this (as
> the rate of window system events is lower than the framerate), this can be programmatically done
> (as indeed done in EGL dEQP tests).  Nvidia for example fails creation of a new swapchain if there
> are already 20 allocated (on desktop, or less than ten on Quadro).  If the backlog of old
> swapchains get larger than a threshold, ANGLE calls `vkQueueWaitIdle()` and destroys the
> swapchains.
