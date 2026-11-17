#include <stdio.h>
#include <string.h>

#include "cmdwords.h"

// The 2D class's PixelsFromMemory registers (source/maxwell/engine_2d.def), on the subchannel deko3d binds it to
// (source/maxwell/helpers.h).
#define SUBCHANNEL_2D 3
#define DST_X0 0x22c
#define DST_Y0 0x22d
#define SRC_X0_INT 0x235
#define SRC_Y0_INT 0x237

typedef struct {
	DkMemBlock mem;
	DkImage image;
	DkImageView view;
} TestImage;

// A block linear image of @p format in a memory block of its own (the words only need its address).
static void makeImage(DkDevice device, TestImage* t, DkImageType type, DkImageFormat format, uint32_t w, uint32_t h,
	uint32_t layers, uint32_t flags)
{
	DkImageLayoutMaker maker;
	dkImageLayoutMakerDefaults(&maker, device);
	maker.flags = flags;
	maker.type = type;
	maker.format = format;
	maker.dimensions[0] = w;
	maker.dimensions[1] = h;
	maker.dimensions[2] = layers;
	DkImageLayout layout;
	dkImageLayoutInitialize(&layout, &maker);
	uint32_t size = (uint32_t)((dkImageLayoutGetSize(&layout) + DK_MEMBLOCK_ALIGNMENT - 1) & ~(uint64_t)(DK_MEMBLOCK_ALIGNMENT - 1));
	DkMemBlockMaker memMaker;
	dkMemBlockMakerDefaults(&memMaker, device, size);
	memMaker.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
	t->mem = dkMemBlockCreate(&memMaker);
	dkImageInitialize(&t->image, &layout, t->mem, 0);
	dkImageViewDefaults(&t->view, &t->image);
}

static void freeImage(TestImage* t)
{
	dkMemBlockDestroy(t->mem);
}

// A blit of a compressed image moves whole blocks: the source and the destination origins each in their own
// image's blocks.
CHECK(image_blit_compressed_origins)
{
	static Capture cap;
	TestImage src, dst;
	makeImage(device, &src, DkImageType_2D, DkImageFormat_RGBA_BC1, 64, 64, 1, DkImageFlags_Usage2DEngine);
	makeImage(device, &dst, DkImageType_2D, DkImageFormat_RGBA_BC1, 64, 64, 1, DkImageFlags_Usage2DEngine);
	DkImageRect srcRect = { 4, 8, 0, 16, 16, 1 }, dstRect = { 8, 12, 0, 16, 16, 1 };
	captureBegin(cmdbuf, &cap);
	dkCmdBufBlitImage(cmdbuf, &src.view, &srcRect, &dst.view, &dstRect, 0, 0);
	bool ok = captureEnd(cmdbuf, &cap, why, whySize);
	freeImage(&src);
	freeImage(&dst);
	if (!ok)
		return false;
	uint32_t dx, dy, sx, sy;
	if (!lastWrite(&cap, SUBCHANNEL_2D, DST_X0, &dx) || !lastWrite(&cap, SUBCHANNEL_2D, DST_Y0, &dy) ||
		!lastWrite(&cap, SUBCHANNEL_2D, SRC_X0_INT, &sx) || !lastWrite(&cap, SUBCHANNEL_2D, SRC_Y0_INT, &sy))
		return snprintf(why, whySize, "dkCmdBufBlitImage wrote no 2D origin"), false;
	// 4x4 blocks: source (4, 8) is block (1, 2), destination (8, 12) block (2, 3)
	if (dx != 2 || dy != 3 || sx != 1 || sy != 2)
		return snprintf(why, whySize, "source (4, 8) to destination (8, 12) of BC1 images wrote source block (%u, %u) "
			"and destination block (%u, %u), wants (1, 2) and (2, 3)", sx, sy, dx, dy), false;
	return true;
}

static bool copyWords(DkCmdBuf cmdbuf, TestImage* src, TestImage* dst, uint32_t srcZ, uint32_t flags, Capture* cap,
	char* why, size_t whySize)
{
	DkImageRect srcRect = { 0, 0, srcZ, 16, 16, 1 }, dstRect = { 0, 0, 0, 16, 16, 1 };
	captureBegin(cmdbuf, cap);
	dkCmdBufCopyImage(cmdbuf, &src->view, &srcRect, &dst->view, &dstRect, flags);
	return captureEnd(cmdbuf, cap, why, whySize);
}

static bool sameWords(const Capture* a, const Capture* b)
{
	return a->numWords == b->numWords && !memcmp(a->words, b->words, 4 * a->numWords);
}

// A copy from layer 1 of an array: other words than one from layer 0, and the same with FlipZ (one layer).
CHECK(image_copy_source_layer)
{
	static Capture layer0, layer1, layer1Flip;
	TestImage src, dst;
	makeImage(device, &src, DkImageType_2DArray, DkImageFormat_RGBA8_Unorm, 16, 16, 4, 0);
	makeImage(device, &dst, DkImageType_2DArray, DkImageFormat_RGBA8_Unorm, 16, 16, 4, 0);
	bool ok = copyWords(cmdbuf, &src, &dst, 0, 0, &layer0, why, whySize) &&
		copyWords(cmdbuf, &src, &dst, 1, 0, &layer1, why, whySize) &&
		copyWords(cmdbuf, &src, &dst, 1, DkBlitFlag_FlipZ, &layer1Flip, why, whySize);
	freeImage(&src);
	freeImage(&dst);
	if (!ok)
		return false;
	if (sameWords(&layer0, &layer1))
		return snprintf(why, whySize, "the copy from layer 1 made the same %u words as the one from layer 0",
			layer1.numWords), false;
	if (!sameWords(&layer1, &layer1Flip))
		return snprintf(why, whySize, "the one-layer copy from layer 1 made other words with FlipZ"), false;
	return true;
}

// The same for dkCmdBufCopyImageToBuffer with FlipZ.
CHECK(image_copy_to_buffer_flip_layer)
{
	static Capture plain, flip;
	TestImage src;
	makeImage(device, &src, DkImageType_2DArray, DkImageFormat_RGBA8_Unorm, 16, 16, 4, 0);
	DkImageRect srcRect = { 0, 0, 2, 16, 16, 1 };
	DkCopyBuf dst = { 0x10000000, 0, 0 };
	bool ok = true;
	captureBegin(cmdbuf, &plain);
	dkCmdBufCopyImageToBuffer(cmdbuf, &src.view, &srcRect, &dst, 0);
	ok = captureEnd(cmdbuf, &plain, why, whySize);
	if (ok)
	{
		captureBegin(cmdbuf, &flip);
		dkCmdBufCopyImageToBuffer(cmdbuf, &src.view, &srcRect, &dst, DkBlitFlag_FlipZ);
		ok = captureEnd(cmdbuf, &flip, why, whySize);
	}
	freeImage(&src);
	if (!ok)
		return false;
	if (!sameWords(&plain, &flip))
		return snprintf(why, whySize, "the one-layer copy of layer 2 to a buffer made other words with FlipZ"), false;
	return true;
}
