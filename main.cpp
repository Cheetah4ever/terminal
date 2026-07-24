#pragma once
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <pthread.h>

#define MAXSESSION 100
#define BUFFER_SIZE 512
#define oops(msg)    \
    {                \
        perror(msg); \
        exit(1);     \
    }
#define sleep_one_second() sleep(1);

static char **terminal_record = NULL;
static int line_count = 0;
char *makestring(char *buf)
{
    char *cp;
    buf[strlen(buf) - 1] = '\0';
    cp = static_cast<char *>(malloc(strlen(buf) + 1));
    if (cp == NULL)
    {
        fprintf(stderr, "no memory\n");
        exit(1);
    }
    strcpy(cp, buf);
    return cp;
}

static char *session_container[MAXSESSION];
static int sessioncount;

static char *sock_path = "/tmp/mysocket.sock";

char *create_session(char *session_name = "work")
{
    char *new_session;
    new_session = static_cast<char *>(malloc(strlen(sock_path) + strlen(".") + strlen(session_name) + 1));
    strcpy(new_session, sock_path);
    new_session[strlen(sock_path)] = '.';
    for (int i = 0; i < strlen(session_name); i++)
        new_session[strlen(sock_path) + 1 + i] = session_name[i];
    new_session[strlen(new_session)] = '\0';
    return new_session;
}

void list_sessions()
{
    printf("TMUX Sessions:\n");
    for (int i = 0; i < MAXSESSION; i++)
        if (session_container[i] != NULL)
            printf("%s\n", session_container[i]);
}
void kill_session(char *session_name)
{
    for (int i = 0; i < MAXSESSION; i++)
        if (strcmp(session_container[i], session_name) == 0)
            session_container[i] = NULL;
};
void clean_up_all_sessons()
{
    for (int i = 0; i < MAXSESSION; i++)
        session_container[i] = NULL;
};

int record_terminal()
{
    // 1. deine the terminal comman to run
    const char *command = "ls -la";

    // 2. Setup dynamic array

    int array_capacity = 0;

    // 3. Open a pipe to execute the terminal command
    FILE *pipe = popen(command, "r");

    if (pipe == NULL)
        oops("Pipe");

    // 4. Temporary buffer to hold each line
    char buffer[BUFFER_SIZE];

    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
    {

        // if the array is full, increase its capacity
        if (line_count >= array_capacity)
        {
            array_capacity = array_capacity == 0 ? 10 : array_capacity * 2;
            char **temp = static_cast<char **>(realloc(terminal_record, array_capacity * sizeof(char *)));
            if (temp == NULL)
                oops("Temp line record");
            terminal_record = temp;
        }
        // Allocate the exact memory for the string and copy it into the array
        terminal_record[line_count] = static_cast<char *>(malloc(strlen(buffer) + 1));
        if (terminal_record[line_count] == NULL)
            oops("Memory allocation failed");
        strcpy(terminal_record[line_count], buffer);
        line_count++;
    }
}

int display_terminal()
{
    int i = 0;
    for (int i = 0; i < line_count; i++)
        printf("%d: %s", i, terminal_record[i]);
}

struct TerminalSize
{
    int rows;
    int columns;
};

/*
 * A signal handler should do as little work as possible. In particular, it
 * should not call printf() or ioctl(). The handlers below only set flags of
 * type sig_atomic_t; the main loop performs the actual terminal operations.
 *
 * resize_requested starts at 1 so the first loop iteration also performs the
 * same setup used after a real window resize.
 */
static volatile sig_atomic_t resize_requested = 1;
static volatile sig_atomic_t stop_requested = 0;

// SIGWINCH is sent when the user changes the terminal window size.
void handle_resize(int)
{
    resize_requested = 1;
}

// SIGINT (Ctrl+C) and SIGTERM request an orderly exit from the main loop.
void handle_stop(int)
{
    stop_requested = 1;
}

TerminalSize get_terminal_size()
{
    struct winsize w;

    /*
     * TIOCGWINSZ asks the terminal attached to stdout for its current number
     * of rows and columns. Use a conventional default if stdout is not a
     * terminal or the terminal reports an unusable size.
     */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 ||
        w.ws_row == 0 || w.ws_col == 0)
    {
        return {24, 80};
    }

    return {static_cast<int>(w.ws_row), static_cast<int>(w.ws_col)};
}

void configure_scrolling_region(const TerminalSize &size)
{
    /*
     * ESC[r resets scrolling to the entire window. ESC[1;Nr then limits
     * ordinary scrolling to rows 1 through N, where N is the row immediately
     * above the status bar. Output can therefore scroll without moving or
     * overwriting the final row.
     */
    printf("\033[r");

    if (size.rows > 1)
        printf("\033[1;%dr", size.rows - 1);
}

void clear_row(int row)
{
    if (row < 1)
        return;

    printf("\033[s");          // Save the application's cursor position.
    printf("\033[%d;1H", row); // Move to column 1 of the requested row.
    printf("\033[2K");         // Erase the complete row.
    printf("\033[u");          // Return to the saved cursor position.
}

void draw_status_bar(const TerminalSize &size, const char *status_bar)
{
    /*
     * Drawing temporarily moves the cursor to the bottom row. Saving and
     * restoring the cursor lets normal application output continue from
     * exactly where it was before the clock was redrawn.
     */
    printf("\033[s");                // Save the current cursor position.
    printf("\033[%d;1H", size.rows); // Move to the bottom-left corner.
    printf("\033[1;44;37m");         // Bold white text on blue.

    /*
     * The field width fills the row with spaces, while the precision truncates
     * text that is wider than a newly narrowed terminal. Both values come from
     * the current window width rather than a hard-coded width.
     */
    printf("%-*.*s", size.columns, size.columns, status_bar);

    printf("\033[0m"); // Restore the default text style.
    printf("\033[u");  // Restore the application cursor.
    fflush(stdout);
}

void restore_terminal()
{
    /*
     * The program changes terminal-wide state, so atexit() calls this function
     * on a normal exit and after our signal handlers end the loop.
     */
    printf("\033[r");    // Allow the entire terminal to scroll again.
    printf("\033[0m");   // Restore the default colours and attributes.
    printf("\033[?25h"); // Make sure the cursor is visible.
    fflush(stdout);
}

void *display_status_bar_bottom_line(void *)
{
    char status_bar[128];
    int seconds_elapsed = 0;
    TerminalSize terminal_size = get_terminal_size();

    // Arrange for resizing, Ctrl+C, and termination to be handled cleanly.
    signal(SIGWINCH, handle_resize);
    signal(SIGINT, handle_stop);
    signal(SIGTERM, handle_stop);
    atexit(restore_terminal);

    // Reserve the final row before any ordinary output is written.
    configure_scrolling_region(terminal_size);
    printf("\033[2J\033[H"); // Clear the screen and move to the top-left.

    while (!stop_requested)
    {
        if (resize_requested)
        {
            /*
             * Remove the bar at its previous location, obtain the new rows and
             * columns, then reserve the new final row. This keeps the bar at
             * the bottom and makes its background match the new width.
             */
            const TerminalSize new_size = get_terminal_size();
            clear_row(terminal_size.rows);
            terminal_size = new_size;
            configure_scrolling_region(terminal_size);
            resize_requested = 0;
        }

        time_t raw_time;
        struct tm *time_info;
        char time_string[9];
        time(&raw_time);
        time_info = localtime(&raw_time);
        strftime(time_string, sizeof(time_string), "%H:%M:%S", time_info);

        snprintf(status_bar, sizeof(status_bar), "[Clock: %s]", time_string);

        // Redrawing once per iteration also updates the displayed clock.
        draw_status_bar(terminal_size, status_bar);

        // Simulate regular logs popping up on the server over time.
        /*
        if (seconds_elapsed % 3 == 0)
        {
            printf("Some line\n");
            fflush(stdout);
        }

        sleep_one_second();
        */
        seconds_elapsed++;
    }
}

void *exec_command(void *);

int main(int argc, char *argv[])
{
    // record_terminal();
    // display_terminal();
    //exec_command(NULL);
    
    pthread_t t1, t2;

    pthread_create(&t1, NULL, display_status_bar_bottom_line, NULL);
    pthread_create(&t2, NULL, exec_command, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    // display_status_bar_bottom_line();
}

void *exec_command(void *)
{
    char command[128];
    char buffer[512];

    while (fgets(command, sizeof(command), stdin) != NULL)
    {

        if (fputs("Command is: ", stdout) == EOF ||
            fputs(command, stdout) == EOF)
        {
            oops("fputs command");
        }

        FILE *pipe = popen(command, "r");
        if (pipe == NULL)
            oops("Pipe");
            
        while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        {
            if (fputs(buffer, stdout) == EOF)
            {
                pclose(pipe);
                oops("fputs command output");
            }
        }

        if (ferror(pipe))
        {
            pclose(pipe);
            oops("reading command output");
        }
        if (pclose(pipe) == -1)
            oops("pclose");

        fflush(stdout);
    }

    return NULL;
}
