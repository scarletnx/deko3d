#include <stdio.h>
#include <string.h>

#include "cmdwords.h"

// SET_RENDER_ENABLE_A to C of the 3D class, method 0x554 on subchannel 0: the address, high word first, then the
// mode (open-gpu-doc clb197.h; the words a program that writes them by hand sends).
#define RENDER_ENABLE_HEADER 0x20030554u // increasing, 3 words, subchannel 0, method 0x554

static bool expectWords(DkCmdBuf cmdbuf, DkRenderEnableMode mode, DkGpuAddr addr, const uint32_t want[4], char* why,
	size_t whySize)
{
	static Capture cap;
	captureBegin(cmdbuf, &cap);
	dkCmdBufSetRenderEnable(cmdbuf, mode, addr);
	if (!captureEnd(cmdbuf, &cap, why, whySize))
		return false;
	if (cap.numWords != 4 || memcmp(cap.words, want, 16))
		return snprintf(why, whySize, "mode %d at 0x%llx made %u words 0x%08x 0x%08x 0x%08x 0x%08x, wants 0x%08x 0x%08x "
			"0x%08x 0x%08x", (int)mode, (unsigned long long)addr, cap.numWords, cap.words[0], cap.words[1], cap.words[2],
			cap.words[3], want[0], want[1], want[2], want[3]), false;
	return true;
}

// Each mode is one write of the three registers; the unconditional ones with address 0.
CHECK(render_enable_words)
{
	(void)device;
	const DkGpuAddr counters = 0x123456780ull;
	const uint32_t ifEqual[4] = { RENDER_ENABLE_HEADER, 0x1, 0x23456780, 3 };
	const uint32_t ifNotEqual[4] = { RENDER_ENABLE_HEADER, 0x1, 0x23456780, 4 };
	const uint32_t never[4] = { RENDER_ENABLE_HEADER, 0, 0, 0 };
	const uint32_t always[4] = { RENDER_ENABLE_HEADER, 0, 0, 1 };
	return expectWords(cmdbuf, DkRenderEnableMode_IfEqual, counters, ifEqual, why, whySize) &&
		expectWords(cmdbuf, DkRenderEnableMode_IfNotEqual, counters, ifNotEqual, why, whySize) &&
		expectWords(cmdbuf, DkRenderEnableMode_Never, counters, never, why, whySize) &&
		expectWords(cmdbuf, DkRenderEnableMode_Always, DK_GPU_ADDR_INVALID, always, why, whySize);
}
