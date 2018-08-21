#!/bin/bash

# This script renames all the functions and the macros defined in
# absl/base/dynamic_annotations.{h,cc}.
#
# Chromium's dynamic_annotations live in //base/third_party/dynamic_annotations
# and the are in conflict with Abseil's dynamic_annotations (ODR violations and
# macro clashing).
# In order to avoid problems in Chromium, this copy of Abseil has its own
# dynamic_annotations renamed.

for w in \
  AnnotateBarrierDestroy \
  AnnotateBarrierInit \
  AnnotateBarrierWaitAfter \
  AnnotateBarrierWaitBefore \
  AnnotateBenignRace \
  AnnotateBenignRaceSized \
  AnnotateCondVarSignal \
  AnnotateCondVarSignalAll \
  AnnotateCondVarWait \
  AnnotateEnableRaceDetection \
  AnnotateExpectRace \
  AnnotateFlushExpectedRaces \
  AnnotateFlushState \
  AnnotateHappensAfter \
  AnnotateHappensBefore \
  AnnotateIgnoreReadsBegin \
  AnnotateIgnoreReadsEnd \
  AnnotateIgnoreSyncBegin \
  AnnotateIgnoreSyncEnd \
  AnnotateIgnoreWritesBegin \
  AnnotateIgnoreWritesEnd \
  AnnotateMemoryIsInitialized \
  AnnotateMemoryIsUninitialized \
  AnnotateMutexIsNotPHB \
  AnnotateMutexIsUsedAsCondVar \
  AnnotateNewMemory \
  AnnotateNoOp \
  AnnotatePCQCreate \
  AnnotatePCQDestroy \
  AnnotatePCQGet \
  AnnotatePCQPut \
  AnnotatePublishMemoryRange \
  AnnotateRWLockAcquired \
  AnnotateRWLockCreate \
  AnnotateRWLockCreateStatic \
  AnnotateRWLockDestroy \
  AnnotateRWLockReleased \
  AnnotateThreadName \
  AnnotateTraceMemory \
  AnnotateUnpublishMemoryRange \
  GetRunningOnValgrind \
  RunningOnValgrind \
  StaticAnnotateIgnoreReadsBegin \
  StaticAnnotateIgnoreReadsEnd \
  StaticAnnotateIgnoreWritesBegin \
  StaticAnnotateIgnoreWritesEnd \
  ValgrindSlowdown \
; do
  find absl/ -type f -exec sed -i "s/\b$w\b/Absl$w/g" {} \;
done

for w in \
  ADDRESS_SANITIZER_REDZONE \
  ANNOTALYSIS_ENABLED \
  ANNOTATE_BARRIER_DESTROY \
  ANNOTATE_BARRIER_INIT \
  ANNOTATE_BARRIER_WAIT_AFTER \
  ANNOTATE_BARRIER_WAIT_BEFORE \
  ANNOTATE_BENIGN_RACE \
  ANNOTATE_BENIGN_RACE_SIZED \
  ANNOTATE_BENIGN_RACE_STATIC \
  ANNOTATE_CONDVAR_LOCK_WAIT \
  ANNOTATE_CONDVAR_SIGNAL \
  ANNOTATE_CONDVAR_SIGNAL_ALL \
  ANNOTATE_CONDVAR_WAIT \
  ANNOTATE_CONTIGUOUS_CONTAINER \
  ANNOTATE_ENABLE_RACE_DETECTION \
  ANNOTATE_EXPECT_RACE \
  ANNOTATE_FLUSH_EXPECTED_RACES \
  ANNOTATE_FLUSH_STATE \
  ANNOTATE_HAPPENS_AFTER \
  ANNOTATE_HAPPENS_BEFORE \
  ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN ANNOTATE_IGNORE_READS_AND_WRITES_END \
  ANNOTATE_IGNORE_READS_BEGIN \
  ANNOTATE_IGNORE_READS_END \
  ANNOTATE_IGNORE_SYNC_BEGIN \
  ANNOTATE_IGNORE_SYNC_END \
  ANNOTATE_IGNORE_WRITES_BEGIN \
  ANNOTATE_IGNORE_WRITES_END \
  ANNOTATE_MEMORY_IS_INITIALIZED \
  ANNOTATE_MEMORY_IS_UNINITIALIZED \
  ANNOTATE_MUTEX_IS_USED_AS_CONDVAR \
  ANNOTATE_NEW_MEMORY \
  ANNOTATE_NOT_HAPPENS_BEFORE_MUTEX \
  ANNOTATE_NO_OP \
  ANNOTATE_PCQ_CREATE ANNOTATE_PCQ_DESTROY \
  ANNOTATE_PCQ_GET ANNOTATE_PCQ_PUT \
  ANNOTATE_PUBLISH_MEMORY_RANGE \
  ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX \
  ANNOTATE_RWLOCK_ACQUIRED \
  ANNOTATE_RWLOCK_CREATE \
  ANNOTATE_RWLOCK_CREATE_STATIC \
  ANNOTATE_RWLOCK_DESTROY \
  ANNOTATE_RWLOCK_RELEASED \
  ANNOTATE_SWAP_MEMORY_RANGE \
  ANNOTATE_THREAD_NAME \
  ANNOTATE_TRACE_MEMORY \
  ANNOTATE_UNPROTECTED_READ \
  ANNOTATE_UNPUBLISH_MEMORY_RANGE \
  ANNOTATIONS_ENABLED \
  ATTRIBUTE_IGNORE_READS_BEGIN \
  ATTRIBUTE_IGNORE_READS_END \
  DYNAMIC_ANNOTATIONS_ATTRIBUTE_WEAK \
  DYNAMIC_ANNOTATIONS_ENABLED \
  DYNAMIC_ANNOTATIONS_EXTERNAL_IMPL \
  DYNAMIC_ANNOTATIONS_GLUE \
  DYNAMIC_ANNOTATIONS_GLUE0 \
  DYNAMIC_ANNOTATIONS_IMPL \
  DYNAMIC_ANNOTATIONS_NAME \
  DYNAMIC_ANNOTATIONS_PREFIX \
  DYNAMIC_ANNOTATIONS_PROVIDE_RUNNING_ON_VALGRIND \
  DYNAMIC_ANNOTATIONS_WANT_ATTRIBUTE_WEAK \
; do
  find absl/ -type f -exec sed -i "s/\b$w\b/ABSL_$w/g" {} \;
done
