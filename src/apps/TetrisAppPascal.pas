{
  TetrisAppPascal.pas — TETRIS: THE PASCAL EDITION (1992)
  =======================================================
  A faithful port of src/apps/TetrisApp.c, written in strict
  Turbo Pascal 7.0 dialect. Compiles with Free Pascal in
  Turbo Pascal mode:

      fpc -Mtp -Cg -o... TetrisAppPascal.pas

  Why this exists: because we could. The OS already runs C
  without an OS underneath it; now it runs Pascal too. Same
  app contract, same ctx-based state, same zero-alloc rules.
  The compiler just complains in a different language.

  It is NOT linked into the default build. Enabling it is
  opt-in via CMake (CALCOS_ENABLE_PASCAL_TETRIS=ON) so a
  machine without Free Pascal never breaks the project.

  - MTP  = Turbo Pascal 7 dialect (the authentic 1992 vibe)
  - Cg   = PIC codegen so the object links into PIE binaries
  - cdecl exports named pas_tetris_* are consumed by
    TetrisAppPascalBridge.c, which registers this app into
    the kernel's link-time application registry (.appreg),
    exactly like the C apps do.
}

unit TetrisAppPascal;

interface

type
  TColor = LongWord;
  TDisplayDriver = record
    ctx: Pointer;
    width, height, char_width, char_height: LongWord;
    text_cols, text_rows, bpp: LongWord;
    put_char: procedure(self: Pointer; c: Char; x, y: LongWord; fg, bg: TColor);
    write_str: procedure(self: Pointer; s: PChar; x, y: LongWord; fg, bg: TColor);
    set_cursor: procedure(self: Pointer; x, y: LongWord);
    scroll: procedure(self: Pointer; lines: LongInt);
    put_pixel: procedure(self: Pointer; x, y: LongWord; r, g, b: Byte);
    clear: procedure(self: Pointer; bg: TColor);
    draw_line: procedure(self: Pointer; x0, y0, x1, y1: LongInt; color: TColor);
    fill_rect: procedure(self: Pointer; x, y, w, h: LongWord; color: TColor);
    plot_function: procedure(self: Pointer; f: Pointer;
                             xmin, xmax, ymin, ymax: Double; color: TColor);
    present: procedure(self: Pointer);
  end;
  TClipRect = record x, y, w, h: Word; end;

  { Mirrors TetrisAppContext from TetrisApp.c byte-for-byte.
    Layout verified at build time by the C bridge — if you
    touch this record, the bridge's static assert dies. }
  TTetrisCtx = record
    board: array[0..19, 0..9] of Byte;   { 200 }
    cur_x, cur_y: LongInt;               { 8 }
    cur_piece, cur_rotation: LongWord;   { 8 }
    score, ticks: LongWord;              { 8 }
    game_over: Boolean;                  { 1 }
  end;
  PTetrisCtx = ^TTetrisCtx;

  { cdecl + public name = unmangled symbols the C bridge can call.
    Signature matches the kernel Application struct's hooks. }
  procedure pas_tetris_init(ctx: Pointer); cdecl; public name 'pas_tetris_init';
  procedure pas_tetris_update(ctx: Pointer; key: LongWord); cdecl; public name 'pas_tetris_update';
  procedure pas_tetris_draw(ctx: Pointer; disp: Pointer; clip: TClipRect); cdecl; public name 'pas_tetris_draw';

implementation

const
  BOARD_WIDTH  = 10;
  BOARD_HEIGHT = 20;

  { 7 pieces x 4 rotations x 4 blocks x 2 coords. Same table as the
    C version, transcribed by hand. Do NOT rearrange. }
  TETROMINOES: array[0..6, 0..3, 0..3, 0..1] of ShortInt = (
    (((0,1),(1,1),(2,1),(3,1)), ((2,0),(2,1),(2,2),(2,3)), ((0,2),(1,2),(2,2),(3,2)), ((1,0),(1,1),(1,2),(1,3))),
    (((1,1),(1,2),(2,1),(2,2)), ((1,1),(1,2),(2,1),(2,2)), ((1,1),(1,2),(2,1),(2,2)), ((1,1),(1,2),(2,1),(2,2))),
    (((1,0),(0,1),(1,1),(2,1)), ((1,0),(1,1),(2,1),(1,2)), ((0,1),(1,1),(2,1),(1,2)), ((1,0),(0,1),(1,1),(1,2))),
    (((1,1),(2,1),(0,2),(1,2)), ((1,0),(1,1),(2,1),(2,2)), ((1,1),(2,1),(0,2),(1,2)), ((1,0),(1,1),(2,1),(2,2))),
    (((0,1),(1,1),(1,2),(2,2)), ((2,0),(1,1),(2,1),(1,2)), ((0,1),(1,1),(1,2),(2,2)), ((2,0),(1,1),(2,1),(1,2))),
    (((0,0),(0,1),(1,1),(2,1)), ((1,0),(2,0),(1,1),(1,2)), ((0,1),(1,1),(2,1),(2,2)), ((1,0),(1,1),(0,2),(1,2))),
    (((2,0),(0,1),(1,1),(2,1)), ((1,0),(1,1),(1,2),(2,2)), ((0,1),(1,1),(2,1),(0,2)), ((0,0),(1,0),(1,1),(1,2)))
  );

  UI_KEY_NONE  = $00000000;
  UI_KEY_UP    = $00010001;
  UI_KEY_DOWN  = $00010002;
  UI_KEY_LEFT  = $00010003;
  UI_KEY_RIGHT = $00010004;
  UI_KEY_ENTER = $0000000A;

  UI_COLOR_BLACK  = $00000000;
  UI_COLOR_RED    = $00FF0000;
  UI_COLOR_GREEN  = $0000FF00;
  UI_COLOR_YELLOW = $00FFFF00;
  UI_COLOR_CYAN   = $0000FFFF;
  UI_COLOR_GRAY   = $00808080;

{ Deterministic spawn: ticks mod 7. No RNG, no libc, same as C. }
procedure SpawnPiece(g: PTetrisCtx);
var i, px, py: LongInt;
begin
  g^.cur_piece := g^.ticks mod 7;
  g^.cur_rotation := 0;
  g^.cur_x := BOARD_WIDTH div 2 - 2;
  g^.cur_y := 0;

  for i := 0 to 3 do
  begin
    px := g^.cur_x + TETROMINOES[g^.cur_piece][g^.cur_rotation][i][0];
    py := g^.cur_y + TETROMINOES[g^.cur_piece][g^.cur_rotation][i][1];
    if (py >= 0) and (g^.board[py][px] <> 0) then
      g^.game_over := True;
  end;
end;

function CheckCollision(g: PTetrisCtx; dx, dy, dr: LongInt): Boolean;
var next_rot, i, px, py: LongInt;
begin
  next_rot := (g^.cur_rotation + dr) mod 4;
  CheckCollision := False;
  for i := 0 to 3 do
  begin
    px := g^.cur_x + dx + TETROMINOES[g^.cur_piece][next_rot][i][0];
    py := g^.cur_y + dy + TETROMINOES[g^.cur_piece][next_rot][i][1];
    if (px < 0) or (px >= BOARD_WIDTH) or (py >= BOARD_HEIGHT) then
    begin
      CheckCollision := True;
      Exit;
    end;
    if (py >= 0) and (g^.board[py][px] <> 0) then
    begin
      CheckCollision := True;
      Exit;
    end;
  end;
end;

procedure LockPiece(g: PTetrisCtx);
var i, px, py, y, x, sy, sx: LongInt;
    full: Boolean;
begin
  for i := 0 to 3 do
  begin
    px := g^.cur_x + TETROMINOES[g^.cur_piece][g^.cur_rotation][i][0];
    py := g^.cur_y + TETROMINOES[g^.cur_piece][g^.cur_rotation][i][1];
    if py >= 0 then
      g^.board[py][px] := Byte(g^.cur_piece + 1);
  end;

  { Line clearing. The y:=y+1 re-checks the shifted line —
    TP7 has no continue, so this is the honest translation. }
  y := BOARD_HEIGHT - 1;
  while y >= 0 do
  begin
    full := True;
    for x := 0 to BOARD_WIDTH - 1 do
      if g^.board[y][x] = 0 then
      begin
        full := False;
        Break;
      end;
    if full then
    begin
      g^.score := g^.score + 100;
      for sy := y downto 1 do
        for sx := 0 to BOARD_WIDTH - 1 do
          g^.board[sy][sx] := g^.board[sy - 1][sx];
      for sx := 0 to BOARD_WIDTH - 1 do
        g^.board[0][sx] := 0;
      y := y + 1;
    end;
    y := y - 1;
  end;

  SpawnPiece(g);
end;

procedure pas_tetris_init(ctx: Pointer); cdecl;
begin
  FillChar(ctx^, SizeOf(TTetrisCtx), 0);
  SpawnPiece(ctx);
end;

procedure pas_tetris_update(ctx: Pointer; key: LongWord); cdecl;
var g: PTetrisCtx;
begin
  g := ctx;
  g^.ticks := g^.ticks + 1;

  if g^.game_over then
  begin
    if key = UI_KEY_ENTER then pas_tetris_init(ctx);
    Exit;
  end;

  case key of
    UI_KEY_LEFT:
      if not CheckCollision(g, -1, 0, 0) then g^.cur_x := g^.cur_x - 1;
    UI_KEY_RIGHT:
      if not CheckCollision(g, 1, 0, 0) then g^.cur_x := g^.cur_x + 1;
    UI_KEY_DOWN:
      if not CheckCollision(g, 0, 1, 0) then
        g^.cur_y := g^.cur_y + 1
      else
        LockPiece(g);
    UI_KEY_UP:
      if not CheckCollision(g, 0, 0, 1) then
        g^.cur_rotation := (g^.cur_rotation + 1) mod 4;
  else
    { gravity tick every 20 ticks — same cadence as the C app }
    if (g^.ticks mod 20) = 0 then
    begin
      if not CheckCollision(g, 0, 1, 0) then
        g^.cur_y := g^.cur_y + 1
      else
        LockPiece(g);
    end;
  end;
end;

procedure pas_tetris_draw(ctx: Pointer; disp: Pointer; clip: TClipRect); cdecl;
var g: PTetrisCtx;
    d: ^TDisplayDriver;
    offset_x, offset_y, x, y, px, py, i: LongInt;
    cell: Byte;
    color: TColor;
    buf: array[0..63] of Char;
    s: string;
    n: LongInt;
begin
  g := ctx;
  if disp = nil then Exit;
  d := disp;
  if (@d^.clear = nil) or (@d^.put_char = nil) or (@d^.write_str = nil) then Exit;

  d^.clear(d, UI_COLOR_BLACK);

  offset_x := LongInt(clip.x) + 5;
  offset_y := LongInt(clip.y) + 2;

  { borders }
  for y := 0 to BOARD_HEIGHT do
  begin
    d^.put_char(d, '|', LongWord(offset_x - 1), LongWord(offset_y + y), UI_COLOR_GRAY, UI_COLOR_BLACK);
    d^.put_char(d, '|', LongWord(offset_x + BOARD_WIDTH), LongWord(offset_y + y), UI_COLOR_GRAY, UI_COLOR_BLACK);
  end;
  for x := 0 to BOARD_WIDTH - 1 do
    d^.put_char(d, '-', LongWord(offset_x + x), LongWord(offset_y + BOARD_HEIGHT), UI_COLOR_GRAY, UI_COLOR_BLACK);

  { settled blocks }
  for y := 0 to BOARD_HEIGHT - 1 do
    for x := 0 to BOARD_WIDTH - 1 do
    begin
      cell := g^.board[y][x];
      if cell > 0 then
      begin
        color := UI_COLOR_CYAN + TColor(cell - 1);
        d^.put_char(d, '#', LongWord(offset_x + x), LongWord(offset_y + y), color, UI_COLOR_BLACK);
      end;
    end;

  { falling piece }
  if not g^.game_over then
    for i := 0 to 3 do
    begin
      px := g^.cur_x + TETROMINOES[g^.cur_piece][g^.cur_rotation][i][0];
      py := g^.cur_y + TETROMINOES[g^.cur_piece][g^.cur_rotation][i][1];
      if py >= 0 then
        d^.put_char(d, '@', LongWord(offset_x + px), LongWord(offset_y + py), UI_COLOR_YELLOW, UI_COLOR_BLACK);
    end;

  { TP7 strings are ShortStrings — null-terminate into a buffer
    before handing them to the C driver. }
  Str(g^.score, s);
  s := 'SCORE: ' + s;
  for n := 1 to Length(s) do buf[n - 1] := s[n];
  buf[Length(s)] := #0;
  d^.write_str(d, @buf[0], LongWord(offset_x + BOARD_WIDTH + 4), LongWord(offset_y + 2), UI_COLOR_GREEN, UI_COLOR_BLACK);

  if g^.game_over then
  begin
    d^.write_str(d, ' GAME OVER ', LongWord(offset_x + BOARD_WIDTH + 4), LongWord(offset_y + 5), UI_COLOR_RED, UI_COLOR_BLACK);
    d^.write_str(d, 'Press Enter to Retry', LongWord(offset_x + BOARD_WIDTH + 4), LongWord(offset_y + 6), UI_COLOR_YELLOW, UI_COLOR_BLACK);
  end;
end;

end.
