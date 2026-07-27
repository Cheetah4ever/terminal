# terminal++

`terminal++` is an experimental terminal program with a persistent status bar.
The status bar currently displays a clock; over time, it will grow to show more
useful information and provide more terminal tools.

The project is also an exploration of how terminal programs work. Even simple
operations—moving the cursor, reserving the last row, reacting to a resized
window, or restoring the terminal on exit—use compact control sequences and
Unix APIs that can be difficult to decode.

## Current work

The program currently explores:

- reserving the bottom row for a status bar;
- drawing and clearing rows with terminal control sequences;
- reading the current window size with `ioctl()` and `TIOCGWINSZ`;
- responding to `SIGWINCH`, `SIGINT`, and `SIGTERM`;
- restoring terminal state when the program exits;
- running commands while the status bar is displayed; and
- separating command input and terminal output work across threads.

This is an early-stage learning project, not yet a replacement for a shell or a
terminal multiplexer.

## Terminal control guide

See [`terminal.md`](terminal.md) for:

- a plain-language explanation of the control sequences used by the program;
- details of `restore_terminal()`, `clear_row()`, signal handling, resizing,
  scrolling regions, and status-bar drawing;
- a guide to the Unix APIs used alongside those sequences; and
- authoritative references and other tools for building terminal interfaces.

## Build

On macOS or another Unix-like system with a C++ compiler and POSIX threads:

```sh
c++ -std=c++17 -pthread main.cpp -o terminal-plus
./terminal-plus
```

Press <kbd>Ctrl</kbd>+<kbd>C</kbd> to request a clean exit.

## Direction

Possible future status-bar information includes the current directory, command
status, active session, system load, time, and network state. The terminal
output and resize handling will also continue to evolve as the project grows.
