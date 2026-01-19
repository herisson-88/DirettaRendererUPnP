# SDK 148 Migration Journal

This document tracks issues encountered during migration from Diretta Host SDK 147 to SDK 148, along with analysis of responsibility (our code vs SDK behavior).

---

## Issue #1: Use-After-Free in Worker Thread

**Date:** 2026-01-19
**Status:** FIXED
**Commit:** `7a15b42`

### Symptom
Segmentation fault during track changes (DSD→PCM, rate changes, user-initiated skip).

### Root Cause Analysis

**Our code ordering (wrong):**
```cpp
DIRETTA::Sync::close();  // SDK resources freed
m_running = false;       // Worker still running!
m_workerThread.join();   // Worker may have already crashed
```

**Fixed ordering:**
```cpp
m_running = false;       // Signal worker to stop
m_workerThread.join();   // Wait for worker to exit cleanly
DIRETTA::Sync::close();  // NOW safe to free SDK resources
```

### Responsibility Assessment

| Aspect | Responsibility |
|--------|---------------|
| Worker thread lifecycle management | **OURS** - We create/manage the worker thread |
| Proper shutdown sequencing | **OURS** - We must stop consumers before freeing resources |
| SDK documenting thread-safety requirements | SDK supplier (unclear if documented) |

**Verdict:** **OUR BUG** - Classic use-after-free. We should always stop threads before freeing resources they access, regardless of SDK version.

**Why it manifested with SDK 148:** Possibly different timing, memory layout, or SDK 148 does more aggressive cleanup in `close()`.

---

## Issue #2: resize_noremap() Crash on Fresh Streams

**Date:** 2026-01-19
**Status:** FIXED (workaround)
**Commit:** `d8164ce`

### Symptom
Segmentation fault in `DIRETTA::Stream::resize_noremap()` when called on Stream objects after `reopenForFormatChange()`.

### Root Cause Analysis

We attempted to use SDK 148's new `resize_noremap()` API to optimize the hot path:

```cpp
// Our attempted optimization
if (!stream.resize_noremap(currentBytesPerBuffer)) {
    stream.resize(currentBytesPerBuffer);  // Fallback
}
```

Crash occurred in `std::vector::_M_default_append()` called from `resize_noremap()`, indicating the internal vector was uninitialized.

### Responsibility Assessment

| Aspect | Responsibility |
|--------|---------------|
| Using new API correctly | **OURS** - We chose to use resize_noremap() |
| API behavior on uninitialized streams | **SDK** - resize_noremap() should either work or return false |
| Documenting API preconditions | **SDK** - Should document when resize_noremap() is safe |

**Verdict:** **SHARED** - We made an assumption about when `resize_noremap()` is safe. SDK could provide clearer documentation or safer API behavior.

**Workaround:** Reverted to standard `resize()` which handles all cases safely.

**Future investigation:** Check if `resize_noremap()` requires the stream to have been previously sized via `resize()` first.

---

## Issue #3: Double setSink() Corruption (CURRENT)

**Date:** 2026-01-19
**Status:** FIX PENDING (not yet committed)

### Symptom
Segmentation fault in `DIRETTA::Stream::resize()` (standard resize, not resize_noremap) immediately after `reopenForFormatChange()` when worker thread first accesses streams.

### Root Cause Analysis

Our `reopenForFormatChange()` function was calling:
1. `DIRETTA::Sync::close()`
2. `DIRETTA::Sync::open()`
3. `setSink(targetAddress, cycleTime_OLD, ...)` ← First setSink with cached cycleTime
4. `inquirySupportFormat()`

Then the caller continued with:
5. `fullReset()` - clears our internal state
6. `configureRingDSD()` - sets up format
7. Calculate NEW cycleTime based on actual format
8. `setSink(targetAddress, cycleTime_NEW, ...)` ← Second setSink with different cycleTime
9. `connect()` / `play()`

The double `setSink()` calls with different parameters appears to corrupt SDK 148's internal stream state.

### Evidence

- Crash occurs in `std::vector::_M_default_append()` via `DIRETTA::Stream::resize()`
- Stream object's internal vector is in invalid state (garbage pointers)
- Crash happens on FIRST `getNewStream()` call after reopen
- Problem only manifests with SDK 148, not SDK 147

### Responsibility Assessment

| Aspect | Responsibility |
|--------|---------------|
| Calling setSink() twice with different params | **OURS** - Redundant/inconsistent API usage |
| SDK handling multiple setSink() calls gracefully | **SDK** - Should either work or return error, not corrupt state |
| Our code structure separating reopen from configure | **OURS** - Poor separation of concerns |

**Verdict:** **SHARED, leaning OURS**

- We should not have called `setSink()` in `reopenForFormatChange()` when the caller was going to call it again with proper parameters
- However, SDK 148 should not corrupt internal state on redundant API calls - it should either accept the new config or return an error

### Fix Applied (pending commit)

Removed `setSink()` and `inquirySupportFormat()` from `reopenForFormatChange()`. The function now only handles SDK lifecycle:
- `close()` → wait → `open()`

All configuration (`setSink()`, `inquirySupportFormat()`, etc.) is now done once by the caller with correct parameters.

---

## SDK 148 vs SDK 147 Behavioral Differences

Based on our migration experience, SDK 148 appears to have:

1. **Stricter state management** - Internal state corruption is more likely with inconsistent API usage
2. **Different stream object lifecycle** - Fresh streams after reopen may not be in the same state as SDK 147
3. **New APIs (resize_noremap)** - Have undocumented preconditions
4. **Possibly different threading model** - Timing-sensitive race conditions manifest differently

---

## Recommendations for SDK Supplier

1. **Document thread-safety requirements** for `close()` - specifically that all threads accessing SDK resources must be stopped first

2. **Document `resize_noremap()` preconditions** - when is it safe to use vs standard `resize()`?

3. **Make `setSink()` idempotent or error-reporting** - calling it twice with different parameters should not corrupt internal state

4. **Provide stream validity checking** - API to verify a Stream object is in valid state before use

---

## Recommendations for Our Code

1. **Always stop worker threads before SDK close** - This is basic resource management

2. **Single point of configuration** - Don't split setSink() across multiple functions

3. **Defensive stream handling** - Check stream state before operations if SDK provides such API

4. **Avoid new APIs in hot paths without testing** - resize_noremap() looked promising but had edge cases

---

## Testing Protocol for Future SDK Updates

1. Test track changes (same format → quick resume)
2. Test format changes (PCM↔DSD, rate changes)
3. Test user-initiated track skip (EXPERIMENTAL: Force full reopen)
4. Run under GDB/valgrind for memory issues
5. Test high-rate formats (DSD512) which stress timing
