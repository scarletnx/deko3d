#include <stdio.h>
#include <string.h>

#include "cmdwords.h"

// DRAW_TEXTURE's twelve registers from method 0x420, as emulators read them (yuzu's maxwell_3d.h DrawTexture):
// destination origin and size in signed 20.12 fixed point, texels a pixel in signed 32.32 (low word first), the
// sampler, the image, the source origin in 20.12.
#define DRAW_TEXTURE_HEADER 0x200C0420u // increasing, 12 words, subchannel 0, method 0x420

static bool expectWords(DkCmdBuf cmdbuf, DkResHandle handle, DkDrawTextureRegion dst, DkDrawTextureRegion src,
	const uint32_t want[13], char* why, size_t whySize)
{
	static Capture cap;
	captureBegin(cmdbuf, &cap);
	dkCmdBufDrawTexture(cmdbuf, handle, &dst, &src);
	if (!captureEnd(cmdbuf, &cap, why, whySize))
		return false;
	if (cap.numWords != 13)
		return snprintf(why, whySize, "%u words, wants 13", cap.numWords), false;
	for (int i = 0; i < 13; i ++)
		if (cap.words[i] != want[i])
			return snprintf(why, whySize, "destination (%g, %g)-(%g, %g), source (%g, %g)-(%g, %g): word %d is 0x%08x, "
				"wants 0x%08x", dst.x0, dst.y0, dst.x1, dst.y1, src.x0, src.y0, src.x1, src.y1, i, cap.words[i], want[i]),
				false;
	return true;
}

// One texel a pixel: the words a program that draws a 16 by 16 texture at (40, 24) by hand writes.
CHECK(drawtex_one_to_one)
{
	(void)device;
	const uint32_t want[13] = { DRAW_TEXTURE_HEADER, 40 << 12, 24 << 12, 16 << 12, 16 << 12, 0, 1, 0, 1, 3, 5, 0, 0 };
	return expectWords(cmdbuf, dkMakeTextureHandle(5, 3), (DkDrawTextureRegion){ 40, 24, 56, 40 },
		(DkDrawTextureRegion){ 0, 0, 16, 16 }, want, why, whySize);
}

// Scaled and offset: half a texel a pixel across, two down, from a source corner left of and above the texture.
CHECK(drawtex_scaled_negative_origin)
{
	(void)device;
	const uint32_t want[13] = { DRAW_TEXTURE_HEADER, 0x1800, 0x400, 32 << 12, 8 << 12, 0x80000000u, 0, 0, 2,
		7, 0x12345, (uint32_t)-0x2800, (uint32_t)-0x1000 };
	return expectWords(cmdbuf, dkMakeTextureHandle(0x12345, 7), (DkDrawTextureRegion){ 1.5f, 0.25f, 33.5f, 8.25f },
		(DkDrawTextureRegion){ -2.5f, -1, 13.5f, 15 }, want, why, whySize);
}
