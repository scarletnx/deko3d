#include <stdio.h>
#include <stdlib.h>

#include "cmdwords.h"

// Recording calls of a list until the command buffer needs several control memory chunks, then clearing it, over
// and over: the chunks go back to the command buffer's free list, so the device's live allocations stay the same
// from the second round on.
CHECK(memory_cleared_control_chunks_reused)
{
	DkCmdBufMaker maker;
	dkCmdBufMakerDefaults(&maker, device);
	DkCmdBuf callee = dkCmdBufCreate(&maker), caller = dkCmdBufCreate(&maker);
	DkMemBlockMaker memMaker;
	dkMemBlockMakerDefaults(&memMaker, device, 0x10000);
	memMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
	DkMemBlock mem = dkMemBlockCreate(&memMaker);
	dkCmdBufAddMemory(callee, mem, 0, 0x8000);
	dkCmdBufAddMemory(caller, mem, 0x8000, 0x8000);
	dkCmdBufBarrier(callee, DkBarrier_None, DkInvalidateFlags_Image);
	DkCmdList list = dkCmdBufFinishList(callee);

	int32_t live[4];
	for (int round = 0; round < 4; round ++)
	{
		// A call is one control command; 300 of them fill several of the ~1 KiB chunks
		for (int i = 0; i < 300; i ++)
			dkCmdBufCallList(caller, list);
		dkCmdBufClear(caller);
		dkCmdBufAddMemory(caller, mem, 0x8000, 0x8000);
		live[round] = liveAllocations();
	}
	dkCmdBufDestroy(caller);
	dkCmdBufDestroy(callee);
	dkMemBlockDestroy(mem);
	(void)cmdbuf;
	if (live[3] != live[1])
		return snprintf(why, whySize, "the device's live allocations after each clear: %d, %d, %d, %d", live[0], live[1],
			live[2], live[3]), false;
	return true;
}

// A command that ends exactly where the capture storage ends fits in it.
CHECK(memory_command_fills_capture_exactly)
{
	(void)device;
	static uint32_t storage[8];
	static const uint32_t words[8] = { 0x20070000, 1, 2, 3, 4, 5, 6, 7 }; // one increasing write of 7 words
	dkCmdBufBeginCaptureCmds(cmdbuf, storage, 8);
	dkCmdBufReplayCmds(cmdbuf, words, 8);
	uint32_t n = dkCmdBufEndCaptureCmds(cmdbuf);
	if (n != 8)
		return snprintf(why, whySize, "8 words replayed into 8 words of capture storage captured %u", n), false;
	return true;
}

static uint32_t s_addMemCalls;

static void countAddMem(void* userData, DkCmdBuf cmdbuf, size_t minReqSize)
{
	s_addMemCalls ++;
	dkCmdBufAddMemory(cmdbuf, (DkMemBlock)userData, 0x1000, (uint32_t)((minReqSize + 0xFFF) & ~(size_t)0xFFF));
}

// The same with command memory: a command that ends exactly at the end of the slice asks for no more.
CHECK(memory_command_fills_slice_exactly)
{
	(void)cmdbuf;
	static uint32_t words[1024];
	DkMemBlockMaker memMaker;
	dkMemBlockMakerDefaults(&memMaker, device, 0x10000);
	memMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
	DkMemBlock mem = dkMemBlockCreate(&memMaker);
	DkCmdBufMaker maker;
	dkCmdBufMakerDefaults(&maker, device);
	maker.userData = mem;
	maker.cbAddMem = countAddMem;
	DkCmdBuf filled = dkCmdBufCreate(&maker);
	dkCmdBufAddMemory(filled, mem, 0, 0x1000);
	words[0] = 0x23FF0000; // one increasing write of the 1023 words after it
	s_addMemCalls = 0;
	dkCmdBufReplayCmds(filled, words, 1024);
	uint32_t calls = s_addMemCalls;
	dkCmdBufDestroy(filled);
	dkMemBlockDestroy(mem);
	if (calls)
		return snprintf(why, whySize, "1024 words into a 4096-byte slice called the add-memory callback %u times", calls),
			false;
	return true;
}

// Making and destroying a 128 MiB code block 40 times, 5 GiB in all, in deko3d's 4 GiB code segment: each one
// destroyed gives its pages back.
CHECK(memory_code_segment_reused)
{
	(void)cmdbuf;
	const uint32_t size = 128u << 20;
	void* storage = aligned_alloc(DK_MEMBLOCK_ALIGNMENT, size);
	if (!storage)
		return snprintf(why, whySize, "no 128 MiB for the code blocks' storage"), false;
	int made = 0;
	for (; made < 40; made ++)
	{
		DkMemBlockMaker memMaker;
		dkMemBlockMakerDefaults(&memMaker, device, size);
		memMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
		memMaker.storage = storage;
		DkMemBlock mem = dkMemBlockCreate(&memMaker);
		if (!mem)
			break;
		dkMemBlockDestroy(mem);
	}
	free(storage);
	if (made != 40)
		return snprintf(why, whySize, "code block %d of 40 (128 MiB each) could not be made: %s", made + 1, lastError()),
			false;
	return true;
}
