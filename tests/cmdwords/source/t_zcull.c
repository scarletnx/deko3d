#include <stdio.h>

#include "cmdwords.h"

// The 3D class's INVALIDATE_ZCULL (byte 0x1958) and CLEAR_ZCULL_REGION (byte 0x12c8), open-gpu-doc clb197.h. Writing
// 0 to the second clears no region: Z_ENABLE and STENCIL_ENABLE are its bits 0 and 4.
#define SUBCHANNEL_3D 0
#define INVALIDATE_ZCULL 0x656
#define CLEAR_ZCULL_REGION 0x4B2

// A barrier that asks for the zcull to be invalidated writes INVALIDATE_ZCULL, not an empty region clear.
CHECK(zcull_barrier_invalidates)
{
	(void)device;
	static Capture cap;
	captureBegin(cmdbuf, &cap);
	dkCmdBufBarrier(cmdbuf, DkBarrier_None, DkInvalidateFlags_Zcull);
	if (!captureEnd(cmdbuf, &cap, why, whySize))
		return false;
	uint32_t value;
	if (lastWrite(&cap, SUBCHANNEL_3D, CLEAR_ZCULL_REGION, &value))
		return snprintf(why, whySize, "the barrier wrote 0x%x to CLEAR_ZCULL_REGION (method 0x4b2)", value), false;
	if (!lastWrite(&cap, SUBCHANNEL_3D, INVALIDATE_ZCULL, &value))
		return snprintf(why, whySize, "the barrier wrote no INVALIDATE_ZCULL (method 0x656)"), false;
	return true;
}
