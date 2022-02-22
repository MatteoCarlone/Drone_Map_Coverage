// compile with gcc master.c -lncurses -o master

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ncurses.h>
#include <time.h>

#define MAX_DRONES 4 // max drones number
#define MAX_X 80     // max x value
#define MAX_Y 40     // max y value

// gui markers
#define DRONE '*'
#define EXPLORED '.'

// drone status
#define STATUS_IDLE 0
#define STATUS_ACTIVE 1

// drone message
typedef struct drone_position_t
{
    // timestamp of message
    time_t timestamp;
    // drone status
    int status;
    // x position
    int x;
    // y position
    int y;

} drone_position;

// socket file descriptor
int fd_socket;
// socket connection with drones file descriptors
int fd_drones[MAX_DRONES] = {0, 0, 0, 0};
// drones number
int drones_no = 0;
// port number
int portno;

// array of drones position and status
drone_position positions[MAX_DRONES];

int explored_positions[MAX_X][MAX_Y] = {0};

// server and client addresses
struct sockaddr_in server_addr, client_addr;

// flag if the consumer received all the data
bool flag_terminate_process = 0;

// variables for select function
struct timeval td;
fd_set readfds;
fd_set dronesfds;

// pointer to log file
FILE *logfile;

// variable for logging event time
time_t logtime;

// functions headers
int check(int retval);
void close_program(int sig);
void check_new_connection();
bool check_safe_movement(int drone, drone_position request_position);
void check_move_request();
void update_map();
void setup_colors();
void init_console();
bool value_in_array(int array[], int size, int value);

// This macro checks if something failed, exits the program and prints an error in the logfile
#define CHECK(X) (                                                                                                                     \
    {                                                                                                                                  \
        int __val = (X);                                                                                                               \
        (__val == -1 ? (                                                                                                               \
                           {                                                                                                           \
                               logtime = time(NULL);                                                                                   \
                               fprintf(logfile, "%.19s: ERROR (" __FILE__ ":%d) -- %s\n", ctime(&logtime), __LINE__, strerror(errno)); \
                               fflush(logfile);                                                                                        \
                               (errno == EINTR ? (                                                                                     \
                                                     {                                                                                 \
                                                         __val;                                                                        \
                                                     })                                                                                \
                                               : (                                                                                     \
                                                     {                                                                                 \
                                                         fclose(logfile);                                                              \
                                                         endwin();                                                                     \
                                                         printf("\tAn error has been reported on log file.\n");                        \
                                                         exit(-1);                                                                     \
                                                         -1;                                                                           \
                                                     }));                                                                              \
                           })                                                                                                          \
                     : __val);                                                                                                         \
    })

// handler for SIGTERM signal
void close_program(int sig)
{
    if (sig == SIGTERM)
    {
        flag_terminate_process = 1;
    }
    return;
}

// handler for terminal resizing
void handle_resize(int sig)
{

    if (sig == SIGWINCH)
    {
        endwin();
        init_console();
    }
}

// function to check if a value is present in an array
bool value_in_array(int array[], int size, int value)
{
    for (int i = 0; i < size; i++)
    {
        if (array[i] == value)
            return true;
    }
    return false;
}

// check if a new drone wants to connect to the socket
void check_new_connection()
{
    if (drones_no == MAX_DRONES)
        return;
    else
    {

        // set timeout for select
        td.tv_sec = 0;
        td.tv_usec = 1000;

        FD_ZERO(&readfds);
        // add the selected file descriptor to the selected fd_set
        FD_SET(fd_socket, &readfds);

        // take number of request
        int req_no = CHECK(select(FD_SETSIZE + 1, &readfds, NULL, NULL, &td));
        for (int i = 0; i < req_no; i++)
        {
            // define client length
            int client_length = sizeof(client_addr);

            // enstablish connection
            fd_drones[drones_no] = CHECK(accept(fd_socket, (struct sockaddr *)&client_addr, &client_length));
            if (fd_drones[drones_no] < 0)
            {
                CHECK(-1);
            }
            // write on log file
            logtime = time(NULL);
            fprintf(logfile, "%.19s: drone %d connected\n", ctime(&logtime), drones_no + 1);
            fflush(logfile);

            // update new drone status
            positions[drones_no].status = STATUS_ACTIVE;
            drones_no++;
        }
    }
    return;
}

// check if a required movement is safe
bool check_safe_movement(int drone, drone_position request_position)
{
    // check for map edges
    if (request_position.x < 0 || request_position.x > MAX_X || request_position.y < 0 || request_position.y > MAX_Y)
    {
        // write on log file
        logtime = time(NULL);
        fprintf(logfile, "%.19s: drone %d can't go in (%d,%d)\n", ctime(&logtime), drone, request_position.x, request_position.y);
        fflush(logfile);

        return false;
    }

    // for every drone...
    for (int i = 0; i < drones_no; i++)
    {
        if (i != drone)
        {

            // check if the position requested falls in a 3x3 area surrounding another drone
            for (int j = positions[i].x - 1; j <= positions[i].x + 1; j++)
            {
                for (int k = positions[i].y - 1; k <= positions[i].y + 1; k++)
                {
                    // check if there can be a collision between others drones
                    if (j == request_position.x && k == request_position.y)
                    {
                        // write on log file
                        logtime = time(NULL);
                        fprintf(logfile, "%.19s: drone %d can't go in (%d,%d)\n", ctime(&logtime), drone + 1, request_position.x, request_position.y);
                        fflush(logfile);

                        return false;
                    }
                }
            }
        }
    }
    return true;
}

// check if there is a new movement request
void check_move_request()
{
    if (drones_no == 0)
        return;
    else
    {

        // set timeout for select
        td.tv_sec = 0;
        td.tv_usec = 1000;

        FD_ZERO(&dronesfds);
        // add the selected file descriptor to the selected fd_set
        for (int i = 0; i < drones_no; i++)
        {
            FD_SET(fd_drones[i], &dronesfds);
        }

        // take number of request
        int req_no = CHECK(select(FD_SETSIZE + 1, &dronesfds, NULL, NULL, &td));

        // list of managed drone's request
        int managed_request[req_no];

        // for every request
        for (int i = 0; i < req_no; i++)
        {

            // find the drone that has made the request
            for (int j = 0; j < drones_no; j++)
            {
                // if this drone has made a request that is not already managed
                if (FD_ISSET(fd_drones[j], &dronesfds) && !value_in_array(managed_request, i, j))
                {
                    // add to managed queue
                    managed_request[i] = j;

                    // read requested position
                    drone_position request_position;
                    CHECK(read(fd_drones[j], &request_position, sizeof(request_position)));

                    // check drone status, idle or active
                    if (request_position.status == STATUS_IDLE)
                    {
                        // change drone status
                        //  status 1 means active, status 0 means idle. (Initial) status -1 means drone is not connected
                        positions[j].status = STATUS_IDLE;

                        // write on log file
                        logtime = time(NULL);
                        fprintf(logfile, "%.19s: drone %d going idle\n", ctime(&logtime), j + 1);
                        fflush(logfile);

                        // update map
                        update_map();
                    }
                    else if (request_position.status == STATUS_ACTIVE)
                    {
                        // it the drone was previousely idle, update status
                        if (positions[j].status == STATUS_IDLE)
                            positions[j].status = STATUS_ACTIVE;

                        // write on log file
                        logtime = time(NULL);
                        fprintf(logfile, "%.19s: drone %d requests position (%d,%d)\n", ctime(&logtime), j + 1, request_position.x, request_position.y);
                        fflush(logfile);

                        // check if the movement is safe
                        bool verdict = check_safe_movement(j, request_position);
                        // tell the drones if it can move
                        write(fd_drones[j], &verdict, sizeof(verdict));

                        // if the drone can move, update its position
                        if (verdict)
                        {
                            // update new position
                            positions[j].x = request_position.x;
                            positions[j].y = request_position.y;

                            // update explored positions
                            explored_positions[request_position.x][request_position.y] = 1;

                            // update map
                            update_map();
                        }
                    }
                    break;
                }
            }
        }
    }
    return;
}

// update map GUI
void update_map()
{

    // Reset map
    for (int i = 2; i <= MAX_X + 1; i++)
    {
        for (int j = 1; j <= MAX_Y; j++)
        {
            mvaddch(j, i, ' ');
        }
    }

    // print already explored positions in gray
    for (int i = 0; i < MAX_X; i++)
    {
        for (int j = 0; j < MAX_Y; j++)
        {
            if (explored_positions[i][j] == 1)
            {
                // move cursor to the explored position and add marker
                mvaddch(j + 1, i + 2, EXPLORED);
            }
        }
    }

    // enable bold characters
    attron(A_BOLD);

    // print actual drone position
    for (int i = 0; i < drones_no; i++)
    {
        // blue drone are idle
        if (positions[i].status == STATUS_IDLE)
            attron(COLOR_PAIR(2));
        // green drone are active
        else
            attron(COLOR_PAIR(1));

        // print drone and label (under or over the drone)
        mvaddch(positions[i].y + 1, positions[i].x + 2, DRONE);
        if (positions[i].y == 0)
        {
            mvaddch(positions[i].y + 2, positions[i].x + 2, (i + 1) + '0');
        }
        else
        {
            mvaddch(positions[i].y, positions[i].x + 2, (i + 1) + '0');
        }

        // disable colors
        if (positions[i].status == STATUS_IDLE)
            attroff(COLOR_PAIR(2));
        else
            attroff(COLOR_PAIR(1));
    }

    // disable bold characters
    attroff(A_BOLD);

    // print number of connected drones
    move(43, 0);
    printw("Drones connected:  %d\n", drones_no);

    // Send changes to the console.
    refresh();

    return;
}

// setup console colors
void setup_colors()
{

    if (!has_colors())
    {
        endwin();
        printf("This terminal is not allowed to print colors.\n");
        exit(1);
    }

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
}

// Function to initialize the console GUI.
void init_console()
{

    // init console
    initscr();
    refresh();
    clear();
    setup_colors();

    // resize curses terminal
    resize_term(MAX_Y + 4, MAX_X + 4);

    // hide cursor
    curs_set(0);

    // enable bold characters
    attron(A_BOLD);

    // print top wall
    addstr("||");
    for (int j = 2; j < MAX_X + 2; j++)
    {
        mvaddch(0, j, '=');
    }
    addstr("||");

    // for each line...
    for (int i = 0; i < MAX_Y; i++)
    {
        // print left wall
        addstr("||");

        for (int j = 0; j < MAX_X; j++)
            addch(' ');
        // print right wall
        addstr("||");
    }

    // print bottom wall
    addstr("||");
    for (int j = 2; j < MAX_X + 2; j++)
        mvaddch(MAX_Y + 1, j, '=');
    addstr("||");

    // disable bold characters
    attroff(A_BOLD);

    // print number of connected drones
    printw("\nDrones connected:  %d", drones_no);

    // Send changes to the console.
    refresh();
}

int main(int argc, char *argv[])
{
    // init console GUI
    init_console();

    // handle terminal resize signal
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_resize;
    sa.sa_flags = SA_RESTART;
    CHECK(sigaction(SIGWINCH, &sa, NULL));

    // open log file in write mode
    logfile = fopen("logfile/log_master_dpm403.txt", "w");
    if (logfile == NULL)
    {
        endwin();
        printf("an error occured while creating master's log File\n");
        return 0;
    }

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "\n%.19s: starting master\n", ctime(&logtime));
    fflush(logfile);

    // getting port number
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(0);
    }
    portno = atoi(argv[1]);

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: received portno %d\n", ctime(&logtime), portno);
    fflush(logfile);

    // create socket
    fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket < 0)
    {
        CHECK(-1);
    }

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: socket created\n", ctime(&logtime));
    fflush(logfile);

    // set server address for connection
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portno);

    // bind socket
    CHECK(bind(fd_socket, (struct sockaddr *)&server_addr,
               sizeof(server_addr)));

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: socket bound\n", ctime(&logtime));
    fflush(logfile);

    // wait for connections
    CHECK(listen(fd_socket, 5));

    while (!flag_terminate_process)
    {
        // check if a new drone send a connection request
        check_new_connection();

        // check if some drones want to move
        check_move_request();
    }

    // close ncurses console
    endwin();

    // close sockets
    CHECK(close(fd_socket));
    for (int i = 0; i < drones_no; i++)
    {
        CHECK(close(fd_drones[i]));
    }

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: all socket closed\n", ctime(&logtime));
    fflush(logfile);

    // close log file
    fclose(logfile);

    return 0;
}