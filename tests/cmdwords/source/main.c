#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "cmdwords.h"

static Check* s_checks;
static uint32_t s_errorCount;
static char s_lastError[256];
static int32_t s_liveAllocations;

static jmp_buf s_checkFailed;
static bool s_inCheck;

// deko3d calls this on an error and, when it returns, its own handler, which ends the program: inside a check the
// error ends the check instead (the half-made objects are left as they are).
static void recordError(void* userData, const char* context, DkResult result, const char* message)
{
	(void)userData;
	s_errorCount ++;
	snprintf(s_lastError, sizeof(s_lastError), "%s: %s (DkResult %d)", context, message, (int)result);
	if (s_inCheck)
		longjmp(s_checkFailed, 1);
}

static DkResult countedAlloc(void* userData, size_t alignment, size_t size, void** out)
{
	(void)userData;
	*out = aligned_alloc(alignment, (size + alignment - 1) & ~(alignment - 1));
	if (!*out)
		return DkResult_OutOfMemory;
	s_liveAllocations ++;
	return DkResult_Success;
}

static void countedFree(void* userData, void* mem)
{
	(void)userData;
	if (mem)
		s_liveAllocations --;
	free(mem);
}

uint32_t errorCount(void)
{
	return s_errorCount;
}

const char* lastError(void)
{
	return s_lastError;
}

int32_t liveAllocations(void)
{
	return s_liveAllocations;
}

void registerCheck(Check* check)
{
	// Sorted by name, so the output's order does not depend on the link order
	Check** at = &s_checks;
	while (*at && strcmp((*at)->name, check->name) < 0)
		at = &(*at)->next;
	check->next = *at;
	*at = check;
}

void captureBegin(DkCmdBuf cmdbuf, Capture* cap)
{
	memset(cap, 0, sizeof(*cap));
	dkCmdBufBeginCaptureCmds(cmdbuf, cap->words, MAX_WORDS);
}

bool captureEnd(DkCmdBuf cmdbuf, Capture* cap, char* why, size_t whySize)
{
	cap->numWords = dkCmdBufEndCaptureCmds(cmdbuf);
	for (uint32_t i = 0; i < cap->numWords;)
	{
		uint32_t header = cap->words[i++];
		uint32_t method = header & 0x1FFF, subchannel = (header >> 13) & 7, arg = (header >> 16) & 0x1FFF;
		uint32_t mode = header >> 29;
		if (mode == 4) // inline: the value is in the header
		{
			if (cap->numWrites == MAX_WRITES)
				return snprintf(why, whySize, "more than %u register writes", MAX_WRITES), false;
			cap->writes[cap->numWrites++] = (Write){ subchannel, method, arg };
			continue;
		}
		if (mode != 1 && mode != 3 && mode != 5)
			return snprintf(why, whySize, "word %u, 0x%08x, has submission mode %u", i - 1, header, mode), false;
		if (i + arg > cap->numWords)
			return snprintf(why, whySize, "word %u, 0x%08x, wants %u words and %u are left", i - 1, header, arg,
				cap->numWords - i), false;
		for (uint32_t k = 0; k < arg; k ++)
		{
			if (cap->numWrites == MAX_WRITES)
				return snprintf(why, whySize, "more than %u register writes", MAX_WRITES), false;
			uint32_t m = mode == 1 ? method + k : mode == 3 ? method : method + (k ? 1 : 0);
			cap->writes[cap->numWrites++] = (Write){ subchannel, m, cap->words[i++] };
		}
	}
	return true;
}

bool lastWrite(const Capture* cap, uint32_t subchannel, uint32_t method, uint32_t* value)
{
	for (uint32_t i = cap->numWrites; i --;)
		if (cap->writes[i].subchannel == subchannel && cap->writes[i].method == method)
			return *value = cap->writes[i].value, true;
	return false;
}

// libnx's start, less the services the checks do not use (time, file system, hid)
void __appInit(void)
{
	if (R_FAILED(smInitialize()))
		diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
	if (R_SUCCEEDED(setsysInitialize()))
	{
		SetSysFirmwareVersion firmware;
		if (R_SUCCEEDED(setsysGetFirmwareVersion(&firmware)))
			hosversionSet(MAKEHOSVERSION(firmware.major, firmware.minor, firmware.micro));
		setsysExit();
	}
	if (R_FAILED(appletInitialize()))
		diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_AM));
}

void __appExit(void)
{
	appletExit();
	smExit();
}

static void say(const char* line)
{
	fputs(line, stdout);
	svcOutputDebugString(line, strlen(line));
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	DkDeviceMaker deviceMaker;
	dkDeviceMakerDefaults(&deviceMaker);
	deviceMaker.cbDebug = recordError;
	deviceMaker.cbAlloc = countedAlloc;
	deviceMaker.cbFree = countedFree;
	DkDevice device = dkDeviceCreate(&deviceMaker);
	DkCmdBufMaker cmdbufMaker;
	dkCmdBufMakerDefaults(&cmdbufMaker, device);
	DkCmdBuf cmdbuf = dkCmdBufCreate(&cmdbufMaker);

	unsigned passed = 0, failed = 0;
	for (Check* c = s_checks; c; c = c->next)
	{
		char why[512] = "";
		bool ok;
		s_inCheck = true;
		if (setjmp(s_checkFailed))
		{
			// The command buffer may still be capturing: the next checks get a new one
			snprintf(why, sizeof(why), "deko3d error, %s", s_lastError);
			ok = false;
			cmdbuf = dkCmdBufCreate(&cmdbufMaker);
		}
		else
			ok = c->run(device, cmdbuf, why, sizeof(why));
		s_inCheck = false;
		char line[640];
		snprintf(line, sizeof(line), "%s %s%s%s\n", ok ? "PASS" : "FAIL", c->name, why[0] ? ": " : "", why);
		say(line);
		ok ? passed ++ : failed ++;
	}
	char summary[96];
	snprintf(summary, sizeof(summary), "SUMMARY %u passed, %u failed\n", passed, failed);
	say(summary);

	dkCmdBufDestroy(cmdbuf);
	dkDeviceDestroy(device);
	return failed ? 1 : 0;
}
