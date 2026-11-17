#include <stdio.h>

#include "cmdwords.h"

// The 3D class's IndependentBlend registers of render target 0 (source/maxwell/engine_3d.def): the colour's and
// the alpha's destination factor.
#define SUBCHANNEL_3D 0
#define FUNC_RGB_DST 0x783
#define FUNC_ALPHA_DST 0x786

static bool dstFactors(DkCmdBuf cmdbuf, DkBlendFactor colour, DkBlendFactor alpha, uint32_t* rgbWord,
	uint32_t* alphaWord, char* why, size_t whySize)
{
	static Capture cap;
	DkBlendState state;
	dkBlendStateDefaults(&state);
	state.dstColorBlendFactor = colour;
	state.dstAlphaBlendFactor = alpha;
	captureBegin(cmdbuf, &cap);
	dkCmdBufBindBlendStates(cmdbuf, 0, &state, 1);
	if (!captureEnd(cmdbuf, &cap, why, whySize))
		return false;
	if (!lastWrite(&cap, SUBCHANNEL_3D, FUNC_RGB_DST, rgbWord) || !lastWrite(&cap, SUBCHANNEL_3D, FUNC_ALPHA_DST, alphaWord))
		return snprintf(why, whySize, "dkCmdBufBindBlendStates wrote no destination factor of target 0"), false;
	return true;
}

// A blend state whose alpha has another destination factor than its colour: each register gets its own factor.
CHECK(blend_alpha_dst_factor)
{
	(void)device;
	uint32_t rgbA, alphaA, rgbB, alphaB;
	if (!dstFactors(cmdbuf, DkBlendFactor_One, DkBlendFactor_Zero, &rgbA, &alphaA, why, whySize) ||
		!dstFactors(cmdbuf, DkBlendFactor_Zero, DkBlendFactor_One, &rgbB, &alphaB, why, whySize))
		return false;
	if (alphaA != rgbB || rgbA != alphaB)
		return snprintf(why, whySize, "colour One, alpha Zero wrote rgb 0x%x and alpha 0x%x; colour Zero, alpha One "
			"wrote rgb 0x%x and alpha 0x%x: the alpha's word is not its own factor's", rgbA, alphaA, rgbB, alphaB), false;
	return true;
}
