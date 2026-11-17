#include <math.h>

#include "../dk_device.h"
#include "../cmdbuf_writer.h"

#include "engine_3d.h"

using namespace maxwell;
using namespace dk::detail;

namespace
{
	inline uint32_t fixed20_12(float value)
	{
		return uint32_t(int32_t(lrintf(value * 4096.0f)));
	}
}

void dkCmdBufDrawTexture(DkCmdBuf obj, DkResHandle handle, DkDrawTextureRegion const* dstRegion, DkDrawTextureRegion const* srcRegion)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(dstRegion);
	DK_DEBUG_NON_NULL(srcRegion);
	DK_DEBUG_BAD_INPUT(dstRegion->x1 == dstRegion->x0 || dstRegion->y1 == dstRegion->y0, "empty destination region");
	CmdBufWriter w{obj};
	w.reserve(13);

	float dstW = dstRegion->x1 - dstRegion->x0, dstH = dstRegion->y1 - dstRegion->y0;
	int64_t dxdu = llrint(double(srcRegion->x1 - srcRegion->x0) / dstW * 4294967296.0);
	int64_t dydv = llrint(double(srcRegion->y1 - srcRegion->y0) / dstH * 4294967296.0);
	w << Cmd(3D, DrawTextureDstX0{},
		fixed20_12(dstRegion->x0), fixed20_12(dstRegion->y0), fixed20_12(dstW), fixed20_12(dstH),
		uint32_t(dxdu), uint32_t(uint64_t(dxdu) >> 32), uint32_t(dydv), uint32_t(uint64_t(dydv) >> 32),
		handle >> 20, handle & ((1U << 20) - 1),
		fixed20_12(srcRegion->x0), fixed20_12(srcRegion->y0)
	);
}
