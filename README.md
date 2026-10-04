# Chess

A desktop chess game written in C++17 with Qt. You can play against the
computer or against another person over the network.

## Features

- **Full chess rules**: legal move checking for every piece, check, checkmate,
  stalemate, castling, en passant and pawn promotion (you choose the piece).
- **Draws**: insufficient material, fifty-move rule and threefold repetition.
- **Play against the computer** with three levels: Easy, Medium and Hard.
  You can play White, Black or a random colour.
- **Play against a person over TCP**: one player hosts (White), the other joins
  (Black). Works on one computer with two windows, or across a network.
- **Main menu** with separate pages for computer and network games, and a way
  back to the menu from every screen.
- **Board helpers**: legal moves are shown when you select a piece, the last
  move and a king in check are highlighted, and the board turns so your pieces
  are always at the bottom.
- **Captured pieces panel** (bottom right) showing which pieces each side has
  taken and how many of each type.
- **Move list** (right side) in standard notation (`e4`, `Nf3`, `exd5`, `O-O`,
  `e8=Q+`, `Qxf7#`), with the latest move in bold.
- **Undo**: take back moves. Against the computer it works at once; in a network
  game the opponent is asked and can accept or decline.
- Resign and New game buttons.

## Requirements

- A C++17 compiler (developed with MSVC 2022)
- CMake 3.16 or newer
- Qt 5.15 or newer, or Qt 6, with the **Widgets** and **Network** modules

## Building

With Qt Creator: open `CMakeLists.txt`, pick a Qt kit, and press Run.

From the command line:

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/your_kit
cmake --build build
```

If you change a header and the program starts to crash or behaves strangely,
delete the `build` folder and rebuild everything. A partly rebuilt project can
mix old and new class layouts.

## How to play

1. Start the program. The main menu offers **Play against computer** or
   **Play against player**.
2. **Against the computer**: choose the level and your colour, then press
   *Start game*.
3. **Against a player**: choose *Host a game* or *Join a game*.
   - The host plays White and waits for the opponent.
   - The joiner types the host's address and port, and plays Black.
   - The game starts as soon as the connection is made.
4. Click a piece to select it, then click a highlighted square to move.
   Click the selected piece again to deselect it.
5. Use *Main menu* at any time to leave. If a game is still running you will be
   asked to confirm.

### Taking back moves

- **Undo move** (against the computer) goes back to the last position where it
  was your turn: your move and the computer's answer are both taken back. It is
  not available while the computer is thinking.
- **Ask to undo** (network game) sends a request to your opponent, who sees a
  question and can answer Yes or No. Until the answer arrives your board is
  locked. If the opponent already replied to your move, both moves are taken
  back; if not, only your move is. The request is declined automatically if the
  position has changed in the meantime.
- You cannot undo after a resignation.

### Computer levels

| Level  | Behaviour |
|--------|-----------|
| Easy   | Looks one move ahead, makes noisy choices and sometimes plays a random move. |
| Medium | Looks about three moves ahead with a little noise. |
| Hard   | Searches up to six moves ahead (3 second limit per move) and always plays its best move. |

The computer uses a material and piece-position evaluation with alpha-beta
search. It has no opening book and does not know the earlier history of the
game, so it can repeat positions.

### Network play

- The default port is **5555**.
- By default the host listens on `127.0.0.1` only, which is enough for two
  windows on the same computer.
- To play between two computers, change this line in `network_manager.cpp`
  (function `network_manager::host`) and rebuild the host:

  ```cpp
  if (!server_->listen(QHostAddress::Any, port)) {
  ```

  The joining player then types the host's local IP address (for example
  `192.168.1.20`; run `ipconfig` on the host to find it).
- Allow the program through the Windows firewall (private networks) when it
  asks. Both computers must be on the same network, and the host must press
  *Host a game* first.

Messages are plain text lines, one per message:

```
MOVE <fromRow> <fromCol> <toRow> <toCol> <promotion>
RESIGN
NEWGAME
UNDO_REQ <moves played so far> <half-moves to take back>
UNDO_OK
UNDO_NO
```

Rows and columns are numbered 0 to 7. Row 0 is rank 8 and column 0 is file A.
The promotion value is the piece type number (0 means none).

## Project structure

| File | Purpose |
|------|---------|
| `main.cpp` | Starts the application and shows the main window. |
| `mainwindow.h/.cpp` | Window with the menu pages, the game page and the game flow. |
| `chess_table.h/.cpp` | The board widget: drawing, highlights, mouse input. |
| `game_logic.h/.cpp` | The rules engine. Plain C++, no Qt. |
| `chess_bot.h/.cpp` | The computer player (search and evaluation). Plain C++, no Qt. |
| `bot_thread.h/.cpp` | Runs the computer's search on a worker thread so the window stays responsive. |
| `network_manager.h/.cpp` | TCP host/join and the message protocol. |
| `captured_panel.h/.cpp` | The captured pieces panel. |
| `move_list.h/.cpp` | The move list table (standard notation). |
| `figures.h/.cpp` | Loads the piece images. |
| `Resources.qrc`, `images/` | Piece images. |
| `mainwindow.ui` | Qt Designer file for the main window. |

The rules engine uses these coordinates: row 0 is rank 8 (Black's side), row 7
is rank 1, column 0 is file A.

## Images

The piece pictures are loaded from `Resources.qrc`. The `images/` folder must
contain:

```
blackPawn.png    whitePawn.png
blackRook.png    whiteRook.png
blackHorse.png   whiteHorse.png      (knight)
blackElephant.png whiteElephant.png  (bishop)
blackQueen.png   whiteQueen.png
blackKing.png    whiteKing.png
```

## Not implemented yet

- Draw offers and a game clock
- Saving and loading games
- Opening book for the computer
