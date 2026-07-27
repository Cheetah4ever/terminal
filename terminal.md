# Terminal control notes

Terminal control is spread across several layers. The operating system exposes
terminal devices, signals, and window-size queries; terminal emulators interpret
control sequences; and libraries such as terminfo and ncurses provide more
portable interfaces over terminal differences.

This document decodes the operations currently used by `terminal++` and points
to the larger set of tools available for terminal programming.

## Reading an escape sequence

In the source, `\033` is the octal representation of the ESC character
(`0x1b`). Most sequences used here begin with:

```text
ESC [
```

This two-character introducer is normally called CSI: **Control Sequence
Introducer**. It may be followed by numeric parameters separated by semicolons
and then a final character identifying the operation.

For example:

```cpp
printf("\033[%d;1H", row);
```

With `row == 12`, this emits `ESC [ 12 ; 1 H`. The `H` operation positions the
cursor, so the cursor moves to row 12, column 1. Terminal coordinates are
one-based rather than zero-based.

## Sequences currently used

| C++ representation | Terminal notation | Purpose |
| --- | --- | --- |
| `"\033[r"` | `CSI r` | Reset the scrolling region to the full screen. |
| `"\033[1;%dr"` | `CSI 1 ; n r` | Restrict scrolling to rows 1 through `n`. The program uses `rows - 1` so the status row does not scroll. |
| `"\033[s"` | `CSI s` | Save the current cursor position. |
| `"\033[u"` | `CSI u` | Restore the saved cursor position. |
| `"\033[%d;1H"` | `CSI row ; 1 H` | Move the cursor to column 1 of a selected row. |
| `"\033[2K"` | `CSI 2 K` | Erase the complete current row. |
| `"\033[1;44;37m"` | `CSI 1 ; 44 ; 37 m` | Select bold text, a blue background, and a white foreground. |
| `"\033[0m"` | `CSI 0 m` | Reset colours and text attributes. |
| `"\033[?25h"` | `CSI ? 25 h` | Make the cursor visible. This is a DEC private mode. |
| `"\033[2J"` | `CSI 2 J` | Erase the display. |
| `"\033[H"` | `CSI H` | Move the cursor to its home position, normally row 1, column 1. |

`printf("\033[2J\033[H")` combines the last two operations: it clears the
display and then moves the cursor home.

Not every terminal implements every extension identically. In particular,
cursor save and restore have historical variants. For broader compatibility,
query terminal capabilities through terminfo instead of assuming that every
emulator understands a hard-coded sequence.

## How the status bar works

### `configure_scrolling_region()`

The function first emits `CSI r` to remove an earlier scrolling region. It then
emits `CSI 1 ; n r`, where `n` is the row above the status bar. Normal output
can scroll within that area without scrolling the final row.

The scrolling region is terminal-wide state. It must be reset before the
program exits.

### `clear_row()`

The function performs four steps:

1. save the application's cursor position;
2. move to column 1 of the selected row;
3. erase that entire row; and
4. restore the application's cursor position.

Saving and restoring the cursor prevents status-bar maintenance from leaving
ordinary command output at the wrong location.

### `draw_status_bar()`

The function saves the cursor, moves to the bottom-left corner, enables the
status-bar colours, writes a field as wide as the terminal, resets the styling,
and restores the cursor.

This format operation:

```cpp
printf("%-*.*s", size.columns, size.columns, status_bar);
```

is a `printf()` feature rather than a terminal sequence:

- the first `*` supplies the field width;
- the negative width left-aligns the text;
- the second `*` supplies the maximum number of characters; and
- spaces pad the remainder of the row.

The width fills the status-bar background, while the precision prevents a long
status string from overflowing a narrow window.

### `restore_terminal()`

The program changes state owned by the terminal emulator, and that state can
survive after the process exits. `restore_terminal()` therefore:

- restores full-screen scrolling with `CSI r`;
- resets text attributes with `CSI 0 m`;
- makes the cursor visible with `CSI ? 25 h`; and
- calls `fflush(stdout)` so the restoration bytes are written immediately.

`atexit(restore_terminal)` registers this cleanup for normal process
termination. It does not cover every possible exit: `atexit` handlers do not
run after an uncatchable `SIGKILL`, for example.

## Window resizing and signals

When a terminal window changes size, the operating system normally sends
`SIGWINCH` to the foreground process group. The signal handler sets a flag:

```cpp
static volatile sig_atomic_t resize_requested = 1;

void handle_resize(int)
{
    resize_requested = 1;
}
```

Keeping the handler this small matters. Many library functions, including
formatted I/O, are not async-signal-safe. The normal program flow should
perform the terminal query and redraw later.

`get_terminal_size()` uses:

```cpp
struct winsize size;
ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
```

- `STDOUT_FILENO` is the file descriptor for standard output.
- `ioctl()` performs device-specific operations on a file descriptor.
- `TIOCGWINSZ` requests the terminal's current window dimensions.
- `winsize.ws_row` and `winsize.ws_col` contain its rows and columns.

If a program manages a pseudo-terminal, it may also need `TIOCSWINSZ` to apply
the new dimensions to that PTY and `SIGWINCH` to notify the child application.
Redrawing only the outer status bar does not update a child program's idea of
its terminal size.

## Concurrency and terminal output

A terminal is one shared byte stream. If several threads write escape sequences
and command output to `stdout` concurrently, their bytes can be interleaved.
That can split a cursor-control sequence or place command output inside a status
bar update.

A robust design gives one renderer ownership of `stdout`. Worker threads pass
command output to it through a synchronized queue, pipe, or event mechanism.
The renderer can then erase the status bar, print pending output, process a
resize, and redraw the bar as one ordered update.

Useful synchronization and event tools include:

- POSIX mutexes and condition variables;
- `pipe()` or the self-pipe pattern for waking an event loop from a signal;
- `poll()`, `select()`, or `kqueue()` for waiting on several event sources; and
- timed waits for periodic status information such as a clock.

A timed or event-driven wait is preferable to continuously redrawing in a busy
loop.

## Other terminal-programming tools

### `termios`

The POSIX terminal interface reads and changes terminal input modes. It controls
behaviour such as canonical line input, local echo, signal-generating
characters, and byte-at-a-time input. Programs should save the original
attributes with `tcgetattr()` and restore them with `tcsetattr()`.

### terminfo and `tput`

terminfo is a database of capabilities for different terminal types. It avoids
hard-coding every control sequence. Applications can query it through
`setupterm()`, `tigetstr()`, `tparm()`, and `putp()`. The `tput` command exposes
many of the same capabilities to shell scripts and is also useful for
experimentation.

### ncurses

ncurses provides windows, cursor movement, colour handling, keyboard input, and
efficient screen updates at a higher level. It is usually preferable once an
application becomes a full-screen text user interface.

### Pseudo-terminals

PTY APIs such as `posix_openpt()`, `openpty()`, and `forkpty()` create a
terminal-like connection between a controlling program and a child process.
They are fundamental to terminal multiplexers, remote shells, and programs that
host interactive commands.

### Terminal event APIs

`select()` and `poll()` are portable ways to wait for input without busy
spinning. On macOS and BSD systems, `kqueue()` offers a richer event mechanism.
On Linux, `epoll()` serves a similar role. Signal-aware designs may also use
`pselect()`, `ppoll()`, or a self-pipe.

## References

- [ECMA-48: Control Functions for Coded Character Sets](https://ecma-international.org/publications-and-standards/standards/ecma-48/) — the standard behind CSI cursor, erasure, and SGR operations.
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html) — a detailed catalogue covering ANSI, DEC, and xterm sequences.
- [POSIX General Terminal Interface](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap11.html) — terminal attributes, input processing, special characters, and job control.
- [POSIX `tcgetattr()` and `tcsetattr()`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/tcgetattr.html) — reading and applying terminal attributes.
- [ncurses `terminfo(5)`](https://invisible-island.net/ncurses/man/terminfo.5.html) — the terminal capability database and its capability names.
- [ncurses terminal capability functions](https://invisible-island.net/ncurses/man/curs_terminfo.3x.html) — `setupterm()`, `tigetstr()`, `tparm()`, and related APIs.
- [ncurses manual pages](https://invisible-island.net/ncurses/man/) — ncurses, `tput`, terminfo, and associated tools.
- [Linux man-pages: `ioctl_tty(2)`](https://man7.org/linux/man-pages/man2/ioctl_tty.2.html) — window-size queries and other terminal-specific `ioctl()` operations.
- [Linux man-pages: `pty(7)`](https://man7.org/linux/man-pages/man7/pty.7.html) — pseudo-terminal concepts and APIs.
- [POSIX async-signal-safe functions](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html) — signal handling and the operations permitted from signal handlers.

Useful local manual pages can also be explored with:

```sh
man termios
man ioctl
man signal
man sigaction
man pthreads
man 5 terminfo
man tput
man ncurses
```
