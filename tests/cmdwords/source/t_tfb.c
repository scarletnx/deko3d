#include <stdio.h>

#include "cmdwords.h"

// The 3D class's transform feedback registers of buffer 0, as a program that captures gl_Position into it with raw
// methods writes them (envytools rnndb graph/gf100_3d.xml TFB_BUFFER, TFB_STREAM, TFB_ENABLE, TFB_VARYING_LOCS).
#define SUBCHANNEL_3D 0
#define TFB_BUFFER_ENABLE 0x0E0
#define TFB_BUFFER_ADDR_HIGH 0x0E1
#define TFB_BUFFER_ADDR_LOW 0x0E2
#define TFB_BUFFER_SIZE 0x0E3
#define TFB_BUFFER_OFFSET 0x0E4
#define TFB_STREAM 0x1C0
#define TFB_VARYING_COUNT 0x1C1
#define TFB_STRIDE 0x1C2
#define TFB_ENABLE 0x1D1
#define TFB_VARYING_LOCS 0xA00

static bool expectWrite(const Capture* cap, uint32_t method, uint32_t want, const char* what, char* why, size_t whySize)
{
	uint32_t got;
	if (!lastWrite(cap, SUBCHANNEL_3D, method, &got))
		return snprintf(why, whySize, "%s (method 0x%03x) was not written", what, method), false;
	if (got != want)
		return snprintf(why, whySize, "%s (method 0x%03x) is 0x%x, wants 0x%x", what, method, got, want), false;
	return true;
}

// Buffer 0 capturing gl_Position, 16 bytes a vertex, into 256 bytes: the registers end as the raw writes leave them.
CHECK(tfb_capture_position_registers)
{
	(void)device;
	static Capture cap;
	const DkGpuAddr capture = 0x123456700ull;
	DkTransformFeedbackLayout layout = { 16, 0, 4, { DK_TFB_SLOT_POSITION(0), DK_TFB_SLOT_POSITION(1),
		DK_TFB_SLOT_POSITION(2), DK_TFB_SLOT_POSITION(3) } };
	DkTransformFeedbackLayout const* layouts[] = { &layout };
	DkBufExtents buffer = { capture, 256 };
	captureBegin(cmdbuf, &cap);
	dkCmdBufBindTransformFeedbackLayouts(cmdbuf, layouts, 1);
	dkCmdBufBindTransformFeedbackBuffers(cmdbuf, 0, &buffer, 1);
	dkCmdBufBeginTransformFeedback(cmdbuf);
	if (!captureEnd(cmdbuf, &cap, why, whySize))
		return false;
	return expectWrite(&cap, TFB_BUFFER_ENABLE, 1, "buffer 0's enable", why, whySize) &&
		expectWrite(&cap, TFB_BUFFER_ADDR_HIGH, (uint32_t)(capture >> 32), "buffer 0's address, high", why, whySize) &&
		expectWrite(&cap, TFB_BUFFER_ADDR_LOW, (uint32_t)capture, "buffer 0's address, low", why, whySize) &&
		expectWrite(&cap, TFB_BUFFER_SIZE, 256, "buffer 0's size", why, whySize) &&
		expectWrite(&cap, TFB_BUFFER_OFFSET, 0, "buffer 0's start offset", why, whySize) &&
		expectWrite(&cap, TFB_STREAM, 0, "buffer 0's stream", why, whySize) &&
		expectWrite(&cap, TFB_VARYING_COUNT, 4, "buffer 0's varying count", why, whySize) &&
		expectWrite(&cap, TFB_STRIDE, 16, "buffer 0's stride", why, whySize) &&
		expectWrite(&cap, TFB_VARYING_LOCS, 0x1F1E1D1C, "buffer 0's first four varying slots", why, whySize) &&
		expectWrite(&cap, TFB_ENABLE, 1, "the transform feedback enable", why, whySize);
}

// The end of a capture turns it off, and a buffer bound with no size is disabled.
CHECK(tfb_end_and_unbind)
{
	(void)device;
	static Capture cap;
	DkBufExtents none = { 0, 0 };
	captureBegin(cmdbuf, &cap);
	dkCmdBufEndTransformFeedback(cmdbuf);
	dkCmdBufBindTransformFeedbackBuffers(cmdbuf, 0, &none, 1);
	if (!captureEnd(cmdbuf, &cap, why, whySize))
		return false;
	return expectWrite(&cap, TFB_ENABLE, 0, "the transform feedback enable", why, whySize) &&
		expectWrite(&cap, TFB_BUFFER_ENABLE, 0, "buffer 0's enable", why, whySize);
}
