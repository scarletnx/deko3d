#pragma once
// The command word checks: each t_*.c registers its checks with CHECK, and main.c runs them in the order of
// their names.

#include <stdbool.h>
#include <stdint.h>
#include <deko3d.h>

#define MAX_WORDS 4096
#define MAX_WRITES 4096

// One register write the words make: the method's subchannel, its index in words and the value.
typedef struct {
	uint32_t subchannel, method, value;
} Write;

typedef struct {
	uint32_t words[MAX_WORDS];
	uint32_t numWords;
	Write writes[MAX_WRITES];
	uint32_t numWrites;
} Capture;

typedef struct Check {
	const char* name;
	bool (*run)(DkDevice device, DkCmdBuf cmdbuf, char* why, size_t whySize);
	struct Check* next;
} Check;

void registerCheck(Check* check);

#define CHECK(fn) \
	static bool fn(DkDevice device, DkCmdBuf cmdbuf, char* why, size_t whySize); \
	static Check fn##_check = { #fn, fn, 0 }; \
	__attribute__((constructor)) static void fn##_register(void) { registerCheck(&fn##_check); } \
	static bool fn(DkDevice device, DkCmdBuf cmdbuf, char* why, size_t whySize)

// Starts capturing @p cmdbuf's words into @p cap.
void captureBegin(DkCmdBuf cmdbuf, Capture* cap);

// Ends the capture and decodes its words into register writes (increasing, non-increasing, inline and
// increase-once headers). False with a sentence in @p why when a header is none of these or runs past the words.
bool captureEnd(DkCmdBuf cmdbuf, Capture* cap, char* why, size_t whySize);

// The last value written to @p method of @p subchannel, or false when none was.
bool lastWrite(const Capture* cap, uint32_t subchannel, uint32_t method, uint32_t* value);

// The device (the debug library) reports its errors to the harness (DkDeviceMaker::cbDebug), which ends the check
// that made one: how many it reported, and the last one's words.
uint32_t errorCount(void);
const char* lastError(void);

// The device's allocations (DkDeviceMaker::cbAlloc and cbFree) not freed yet.
int32_t liveAllocations(void);
