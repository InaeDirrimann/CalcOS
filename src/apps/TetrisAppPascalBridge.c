// TetrisAppPascalBridge.c — glue the Pascal edition of Tetris into the kernel.
//
// The Pascal side (TetrisAppPascal.pas) is a strict Turbo Pascal 7 unit that
// exports three cdecl hooks (pas_tetris_init/update/draw) operating on the
// same ctx-based state the C apps use. This bridge is the only C in the
// equation: it declares those externs and registers the app through the
// kernel's link-time registry (.appreg), identical to TetrisApp.c.
//
// The Pascal unit is NOT part of the default build. It needs Free Pascal,
// which not every machine has. Enable it explicitly:
//
//     cmake -DCALCOS_ENABLE_PASCAL_TETRIS=ON ..
//
// The build compiles TetrisAppPascal.pas with `fpc -Mtp -Cg`, then links
// this bridge. Nothing else changes.
#include <stdint.h>
#include <stdbool.h>
#include "../../include/kernel/app.h"

// Pascal exports (TetrisAppPascal.pas, fpc -Mtp, cdecl, unmangled names).
extern void pas_tetris_init(void* ctx);
extern void pas_tetris_update(void* ctx, uint32_t key);
extern void pas_tetris_draw(void* ctx, const DisplayDriver* disp, ClipRect clip);

// Must stay byte-identical to TTetrisCtx in TetrisAppPascal.pas.
// The Pascal unit zeroes it on init, so no constructor needed.
typedef struct {
    uint8_t  board[20][10];
    int32_t  current_x, current_y;
    uint32_t current_piece, current_rotation, score, ticks;
    bool     game_over;
} TetrisPascalState;

// If this ever trips, someone drifted the Pascal record and this struct
// apart — the game would corrupt memory silently. Catch it at build time.
_Static_assert(sizeof(TetrisPascalState) == 225 + 3, "Pascal TTetrisCtx layout mismatch");
_Static_assert(_Alignof(TetrisPascalState) == 4, "Pascal TTetrisCtx alignment mismatch");

static TetrisPascalState g_tetris_pascal_state;

const Application g_tetris_pascal_app = {
    .name    = "Tetris (Pascal '92)",
    .context = &g_tetris_pascal_state,
    .init    = pas_tetris_init,
    .update  = pas_tetris_update,
    .draw    = pas_tetris_draw
};

REGISTER_APPLICATION(g_tetris_pascal_app)
