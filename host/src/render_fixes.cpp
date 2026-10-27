// Rendering fixes for things the game did that RT64 draws differently, called from hooks in
// conker.toml.

#include <SDL.h>

// Light glows (such as the two lights over the Feral Reserve's doors).
//
// func_151408A4 draws a glow over a light, fading it out while something stands in front
// of it. It finds that by having the RDP copy the one pixel of the depth buffer under the
// light into memory (its display list gets the pixel's x and y), then, the next frame,
// decoding that sample (D_80089630: the N64's depth format) and comparing it with the
// light's own depth: more than 31 behind the sample, and the glow fades out. RT64 draws
// depth on the GPU, and that sample doesn't come back as the N64's depth, so glows faded
// out wrongly (lit near the light, gone a little way off). The comparison is skipped: a
// glow on screen is shown.

#include <cstdio>

#include "recomp.h"

// stereo.cpp: whether the played camera is parked, which marks a scripted scene (see
// conker_frame_clear below).
extern bool conker_stereo_play_camera_frozen;

// skip.cpp: the skip button (Backspace and the controller's right shoulder), which
// advances scripted sections (the opening's cutscenes among them).
extern "C" void conker_skip_update(uint8_t* rdram);

// func_151408A4 at 0x15140DB4, just after it compared the depths: $t3 is the light's depth
// ($t8) less the sample's ($a2), $a0 the raw sample. Zero passes the test.
extern "C" void conker_light_glow_depth(uint8_t* rdram, recomp_context* ctx) {
    // TEMP-DEBUG: what the sample holds, to see why the test failed.
    static FILE* log = std::fopen("light_glow_log.txt", "w");
    static int lines = 0;
    if (log != nullptr && lines < 3000) {
        std::fprintf(log, "on screen %d light %d sample %d (raw 0x%04X) diff %d\n", (int)ctx->r5, (int)ctx->r24,
            (int)ctx->r6, (unsigned)(ctx->r4 & 0xFFFF), (int)ctx->r11);
        if ((++lines % 20) == 0) {
            std::fflush(log);
        }
    }
    ctx->r11 = 0;
}

// func_1510FEA0 at 0x1510FFA4 (it starts each frame's display list): $t8 is D_800BE635, which
// decides whether the frame is cleared to black before it is drawn. Level setup clears the
// flag, since the level and sky are meant to cover the screen; where they don't (such as with
// the mouse camera in a wall), the last frame's picture repeated. Clear every frame.
extern "C" void conker_frame_clear(uint8_t* rdram, recomp_context* ctx) {
    // The skip button, read once a frame (skip.cpp).
    conker_skip_update(rdram);

    // This runs as each frame's display list starts. The game's mode marks the opening's
    // cutscene sequence (D_800BE9F0: 0x21 from the first cutscene through the keyhole,
    // 0x1D in the Cock and Plucker, 0x25 on the boot screens): there the played camera
    // and the world's objects sit parked while a camera of the scene's own draws the
    // picture, so the game's sprite depths belong to that parked world
    // (host/src/stereo.cpp sends the flag to RT64).
    {
        constexpr uint32_t game_mode_pointer = 0x800BE9F0;
        constexpr uint32_t game_mode_intro_movie = 0x21;
        // The Cock and Plucker's cutscenes (the dinner, the game's first scene) run in
        // mode 0x1D, but so does free roam inside the pub, where the played camera is
        // live. D_800C35EA[p] is 1 while a section (cutscene/scripted sequence) plays,
        // which separates the two; if a pub cutscene turns out not to run a section,
        // this needs a finer signal (see the scripted log / auto-convergence focus).
        constexpr uint32_t game_mode_pub = 0x1D;
        constexpr uint32_t section_playing_pointer = 0x800C35EA;
        const uint32_t game_mode = (uint32_t)MEM_W(0, (gpr)(int32_t)game_mode_pointer);
        const bool section_playing = MEM_BU(0, (gpr)(int32_t)section_playing_pointer) == 1;
        conker_stereo_play_camera_frozen = (game_mode == game_mode_intro_movie) ||
            ((game_mode == game_mode_pub) && section_playing);
    }

    // TEMP-DEBUG: CONKER_NO_FRAME_CLEAR leaves the game's own choice, to compare.
    static const bool disabled = SDL_getenv("CONKER_NO_FRAME_CLEAR") != nullptr;
    if (!disabled) {
        ctx->r24 = 1;
    }
}

// TEMP-DEBUG: the game copies pixels of the depth buffer into small images mid-frame, then
// reads them with the CPU (switching the colour image to them and back). In widescreen,
// what RT64 drew after those copies lost pieces (doors among them). These switch the copies
// off, to find which one does it.
// func_1510B9D0 at 0x1510BE18: $t2 (camera +0x84 & 8) decides whether func_1512E5F0 samples.
extern "C" void conker_depth_copy_camera(uint8_t* rdram, recomp_context* ctx) {
    static const bool skip = SDL_getenv("CONKER_SKIP_DEPTH_CAMERA") != nullptr;
    if (skip) {
        ctx->r10 = 0;
    }
}
// func_151742EC at 0x1517432C: $t6 (its camera argument) nonzero returns before any copy.
extern "C" void conker_depth_copy_probes(uint8_t* rdram, recomp_context* ctx) {
    static const bool skip = SDL_getenv("CONKER_SKIP_DEPTH_PROBES") != nullptr;
    if (skip) {
        ctx->r14 = 1;
    }
}
