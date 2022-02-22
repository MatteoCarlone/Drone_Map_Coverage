//---------------------------------------NEEDED LIBRARIES-------------------------------------

// Compile with: gcc droneAL.c -lm -lncurses -o drone
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <netdb.h>
#include <sys/socket.h>
#include <math.h>
#include <ncurses.h>


#define MAX_X 80 // Defining x max value as requested.
#define MAX_Y 40 // Defining y max value as requested.

// The CHECK(X) function is usefull to write on shell whenever a system call return an error.
// The function will print the name of the file and the line at which it found the error.
// It will end the check exiting with code 1.

#define CHECK(X) (                                                                                            \
{                                                                                                         \
    int __val = (X);                                                                                      \
    (__val == -1 ? (                                                                                      \
     {                                                                                  \
         fprintf(stderr, "ERROR (" __FILE__ ":%d) -- %s\n", __LINE__, strerror(errno)); \
         exit(-1);                                                                      \
         -1;                                                                            \
     })                                                                                 \
    : __val);                                                                                \
})

//-----------------------------------DRONE POSITION STRUCT-------------------------------------

// Together with the other component of the group we decided to use this struct to send
// to the master process the drone position.

typedef struct drone_position_t
{
    time_t timestamp;

    int status; // status is 1 if the drone is moving and 0 if the battery is over so the drone needs to recharge.

    int x;

    int y;
} drone_position;

//-----------------------------------VARIABLES-------------------------------------

// These variables are needed in order to know from the master process if it is possible to change the drone.
// postion as we desire

drone_position current_pos = {.status = 1, .x = 10, .y = 11}; // Drone actual position (the spawning position is choosen by us).
drone_position next_pos = {.status = 1}; // Next drone position.
drone_position supposed_next = {.status = 1}; // The supposed next drone position.

FILE *logfile; // Logfile.
char buffer[80]; // Buffer.                    
int battery_value = 100; // Variable to take into account the change of battery value (initially set to the max value).
int instruction = 1; // Instruction received from the master via socket (0 if the movement is not allowed, 1 if it is allowed).
int step = 1;
bool direction;  // Flag to manage the exploration direction.
bool environment[40][80] = {}; // Matrix that represents the environment. In our GUI '@' stands for a visited position, '#' for not visited.
                               // The default value for a boolean matrix is false, as you will see later false stand for a not visited position
                               // whereas true stands for an already visited position.


//-----------------------------------FUNCTIONS PROTOTYPES-----------------------------------------


void logfilePrint(char *str);
void colors();
void environment_setup();
double randfrom(double min, double max);
void next_position();
void recharge(int fd);
void signal_handler(int sig);




//-----------------------------------MAIN-------------------------------------

int main(int argc, char * argv[]){

    srand(time(NULL));

    // Thanks to these lines of code we can report on the file.txt (logfile.txt)
    // all the operations we have done. The logfile.txt is created and in fact
    // there is the append inside the function fopen() that is not "a", but "w"-->
    // Creates an empty file for writing. If a file with the same name already exists,
    // its content is erased and the file is considered as a new empty file. Whereas in other processes there would be as append "a"-->
    // Appends to a file. Writing operations, append data at the end of the file.
    // The file is created if it does not exist.

    // Write on the logfile.
    logfile = fopen("logfile/log_drone_al9.txt", "w");
    sprintf(buffer, "PID: %d \n", getpid());
    logfilePrint(buffer); // Print on the logfile the PID.
   
    // Signals that the process can receive.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = SA_RESTART;

    CHECK(sigaction(SIGWINCH, &sa, NULL));

    // Command to correctly create the socket required to communicate with the master.
    int fd;                            
    int portno = atoi(argv[1]); // Port number 
    struct sockaddr_in serv_addr;

    // Open the socket.
    fd = CHECK(socket(AF_INET, SOCK_STREAM, 0)); // Creates a new socket.
    logfilePrint("Socket created succesfully.\n");

    // Set the attributes of serv_addr.
    bzero((char *)&serv_addr, sizeof(serv_addr)); // Sets all arguments of serv_addr to zero.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    CHECK(connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))); // Listen for connection.
    logfilePrint("Connection to master process establihsed succesfully.\n");

    // Commands to correctly setup the screen for the graphic interface.
    initscr(); // To initialize the console used for the interface.
    refresh();
    clear();
    colors(); // To setup the colors.
    environment_setup(); // To setup the environment in which the drone moves.

    int random = 10;  // Stuff used to manage the exploration direction.
    int cicli = 0;

    while(1){
       
        if(battery_value == 0){ // First of all we have to check the battery value.
            recharge(fd); // If the battery value is 0 the drone needs to land and recharge.
            direction = !direction;
        }

        if(cicli == random){ // To change the exploration direction after a random number of loop beetwen 0 and 20.
            direction = !direction;
            cicli = 0;
            random = rand() % 20;
        }

        next_position();
        next_pos.timestamp = time(NULL);  

        CHECK(write(fd, &next_pos, sizeof(drone_position))); // Write in the socket the computed next position.
        logfilePrint("Next position sent to master in order to check it.\n");

        CHECK(read(fd, &instruction, sizeof(int)));
        logfilePrint("Feedback sent from master.\n");

        if(instruction == 0){ // Check the instruction received from the master if it is 0 movement is not allowed.

            attron(COLOR_PAIR(2));
            mvaddstr(44, 0, "Movement not allowed.");
            attroff(COLOR_PAIR(2));
            logfilePrint("The drone is UNABLE to move.\n");
        }

        if(instruction == 1){ // If the movement is allowed the drone moves to the computed next position.

            current_pos = next_pos;
            environment[current_pos.y][current_pos.x] = true;
            attron(COLOR_PAIR(3));
            mvaddch(current_pos.y, current_pos.x, '@');
            attroff(COLOR_PAIR(3));
            logfilePrint("The drone is ABLE to move.\n");
        }

        refresh();

        battery_value--; // At every loop the battery decrease of one unit.
        cicli++;

        usleep(100000);
    }

    fclose(logfile); // Closing logfile.

    return 0;
}


//-----------------------------------FUNCTIONS-----------------------------------------

void logfilePrint(char *str){

    // Print on logfile.

    time_t pres_time = time(NULL); // Present time.
    fprintf(logfile, "%.19s: %s", ctime(&pres_time), str);
    fflush(logfile);
}

// The following two functions are needed to manage the simple graphic interface that we have developed.

void colors(){

    // Function used to setup the colors in the environment.

    if (!has_colors()) { // The has_color() function return true if the terminal is allowed to print colors, false if it isn't.
        endwin(); // The endwin function restore the terminal after Curses activity.
        printf("Your terminal is not allowed to print colors.\n");
        exit(1);
    }

    start_color(); // Must be called to initialize the routine to use color.
    init_pair(1, COLOR_BLUE, COLOR_BLACK); // Routine used to initialize a color pair.
    init_pair(2, COLOR_CYAN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
}


void environment_setup(){

    // Function used to setup the environment with ncurses library.

     for (int i = 0; i < MAX_Y; i++){

        for (int j = 0; j < MAX_X; j++){

            if(environment[i][j] == false){ // The i,j value in the matrix is false if the position is not visited.

                attron(COLOR_PAIR(1));
                mvaddch(i, j, '#'); // add the character # at the position specified by i, j
                attroff(COLOR_PAIR(1));
            }

            else{ // The i,j value in the matrix is true if the position has been visited.

                attron(COLOR_PAIR(3));
                mvaddch(i, j, '@'); // add the character @ at the position specified by i, j
                attroff(COLOR_PAIR(3));
            }
        }
    }
}

void signal_handler(int sig){

    // Function to handle the SIGWINCH signal. The OS send this signal to the process when the size of
    // the terminal changes.

    if (sig == SIGWINCH) {

        // If the size of the terminal changes, clear and restart the grafic interface

        endwin(); // clear the graphic interface
        initscr(); // To initialize the console used for the interface.
        refresh();
        clear();
        colors(); // To setup the colors.
        environment_setup(); // To setup the environment in which the drone moves.
    }
}


double randfrom(double min, double max){

    // Function to generate a random double number in an interval we will see later its utility.

    double range = (max - min);

    double div = RAND_MAX / range;

    return min + (rand() / div);
}


void next_position(){

    // This function is used to compute the next position of the drone in the environment.

    // Compute the supposed next position to check if it is already visited and eventually change it

    if(direction){ // the drone surely moves in the x direction and randomly
                   // moves in the y direction (if randfrom returns a value higher than 0.5)

        supposed_next.x = current_pos.x + step;
        supposed_next.y = round(current_pos.y+randfrom(-abs(step), abs(step)));
    }

    else{ // the drone surely moves in the y direction and randomly
          // moves in the x direction (if randfrom returns a value higher than 0.5)

        supposed_next.y = current_pos.y + step;
        supposed_next.x = round(current_pos.x+randfrom(-abs(step), abs(step)));
    }

    // these next two if statements are needed to try to avoid exploration of already visited positions

    if(environment[supposed_next.y][supposed_next.x] == true){  // check if the position has been already visited,
                                                               //  eventually changes step direction and recompute the
                                                               // supposed next position

        step = -step;

        if(direction){

            supposed_next.x = current_pos.x + step;
            supposed_next.y = round(current_pos.y+randfrom(-abs(step), abs(step)));
        }

        else{

            supposed_next.y = current_pos.y + step;
            supposed_next.x = round(current_pos.x+randfrom(-abs(step), abs(step)));
        }
    }

    if(environment[supposed_next.y][supposed_next.x] == true){ // again check if the position has been already visited,
                                                               //  eventually changes exploration direction and recompute the
                                                               // supposed next position

        direction = !direction;

        if(direction){

            supposed_next.x = current_pos.x + step;
            supposed_next.y = round(current_pos.y+randfrom(-abs(step), abs(step)));
        }

        else{

            supposed_next.y = current_pos.y + step;
            supposed_next.x = round(current_pos.x+randfrom(-abs(step), abs(step)));
        }
    }
    
    // then we have to manage cases in which the drone reaches environment boundaries

    if(supposed_next.x < 0){ // left boundary

        // If the next x position is lower than 0 we reach the left boundary

        step = -step; // change step direction
        supposed_next.x = 0; // so since we can't move on the left we keep the robot in the x 0 position.
    }

    else if(supposed_next.x > MAX_X - 1){ // right boundary

        // We keep the drone in the x MAX_X - 1 position.
        // If the next x position is higher than MAX_X - 1 we reach the right boundary

        step = -step; // change step direction
        supposed_next.x = MAX_X - 1; // so since we can't move on the right we keep the robot in the x MAX_X - 1 position.
    }

    if(supposed_next.y < 0){ // upper boundary

        // if the next y position is lower than 0 we reach the upper boundary.

        supposed_next.y = 0; // So since we can't move up we keep the robot in the y 0 position
        step = -step; 
    }

    else if(supposed_next.y > MAX_Y - 1){ // lower boundary

        // if the next y position is higher than MAX_Y - 1 we reach the lower boundary.

        supposed_next.y = MAX_Y - 1; // So since we can't move on the right we keep the robot in the y MAX_Y - 1 position
        step = -step; 
    }

    if(instruction == 0){ // if the instruction received from the master is 0 the drone can't moves,
                          // so we try to change the step direction and the exploration direction
        step = -step; 
        direction = !direction;
    }

    if(instruction == 1){ // the drone can moves

        next_pos = supposed_next;
    }

}


void recharge(int fd){

    // Function used to simulate the recharging of the drone battery.

    attron(COLOR_PAIR(2));
    mvaddstr(41, 0, "The battery is over, land to recharge."); // Print the string in y = 45, x = 0.
    attroff(COLOR_PAIR(2));
    logfilePrint("The battery is over, land to recharge.\n");
    refresh();

    sleep(2);

    next_pos.timestamp = time(NULL);
    next_pos.status = 0; // Setting the idle status since the drone is recharging
    CHECK(write(fd, &next_pos, sizeof(drone_position)));  


    for(int i=1; i< 51; i++){ // for loop for loading bar 
    attron(COLOR_PAIR(2));
    mvaddstr(41, 0, "                                      ");
    mvaddstr(42, 0, "Charging in progres...");
    mvaddch(43, 0, '[');
    mvaddch(43, 51, ']');
    mvaddch(43, i, '*'); // add char x at y = 43 and x = i to print the loading bar
    attroff(COLOR_PAIR(2));
    refresh();
    usleep(100000);
    }
     // Wait for six seconds to simulate the battery recharging.

    attron(COLOR_PAIR(2));
    mvaddstr(42, 0, "Charge completed.     ");
    mvaddstr(43, 0, "                                                    ");
    attroff(COLOR_PAIR(2));
    refresh();

    battery_value = 100;
    logfilePrint("The charge has been completed.\n");
    next_pos.status = 1; // Setting the active status since the drone's charge has been completed.

    sleep(1);

    attron(COLOR_PAIR(2));
    mvaddstr(42, 0, "                 ");
    attroff(COLOR_PAIR(2));
    refresh();
}

