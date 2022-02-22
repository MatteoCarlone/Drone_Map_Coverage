//* FA00 Group (Conti, Perri) --- Third Assignment of Advanced Robot Programming *//
//* * * * * * * * * * * * *  Academic Year: 2021/2022  * * * * * * * * * * * * * *//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <math.h>
#include <stdbool.h>
#include <netdb.h>
#include <time.h>
#include <ncurses.h>

#define MAX_X 80 // max x value
#define MAX_Y 40 // max y value

// drone status
#define STATUS_IDLE 0
#define STATUS_ACTIVE 1

// Function: CHECK(X)
// This function writes on the shell whenever a sistem call returns any kind of error.
// The function will print the name of the file and the line at which it found the error.
// It will end the check exiting with code 1.
#define CHECK(X) (                                             \
		{                                                          \
			int __val = (X);                                         \
			(__val == -1 ? (                                         \
												 {                                     \
													 fprintf(stderr, "ERROR ("__FILE__   \
																					 ":%d) -- %s\n",     \
																	 __LINE__, strerror(errno)); \
													 exit(EXIT_FAILURE);                 \
													 -1;                                 \
												 })                                    \
									 : __val);                                   \
		})

typedef struct drone_position_t // struct that defines the position of the robot, it is a standard structure among all groups
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

drone_position actual_position = {.status = STATUS_ACTIVE, .x = 30, .y = 30}; // actual drone position
drone_position next_position = {.status = STATUS_ACTIVE};											// next drone position
drone_position delta_pos;																											// unitary steps to be added to the drone position
drone_position prevdelta_pos;																									// previous unitary steps to be added to the drone position

int steps = 0;
int dir_changer;
int battery = 100; // battery initially fully charged
int sockfd;
bool isActive=true;				 // socket file descriptor

// Update of the set of file descriptors for the select() function.
fd_set rset;
struct timeval tv = {0, 0};
int ret;
FILE *logfile;					 // logfile
char buf[100];					 // string buffer
int time_speed = 100000; // variable used for setting the drone speed
int nsteps = 10;				 // variable that defines the number of steps the drone will have to do in the same direction, initially it is equal to 10

int tmp_x = 0;
int tmp_y = 0;

bool map[40][80] = {};

void setup_map();							// creates the structure of the GUI
void setup_colors();					// defining the colors for the GUI
void signal_handler(int sig); // signal handler used for SIGWINCH signal (i.e. it manages a screen resize)
void print_logfile();					// print on the logfile
void change_direction();			// randomly changes the drone direction
void next_pos();							// compute the next position of the drone
void set_speed();							// set a speed among the ones that are available
void recharging();						// recharging the drone
void seek_free_pos();

int main(int argc, char *argv[])
{
	srand(time(NULL)); // setting the seed of rand() such as everytime the process starts it will generate a new pattern

	// Open and write on the log file.
	logfile = fopen("logfile/log_drone_FA00.txt", "w");
	sprintf(buf, "Process created, PID: %d \n", getpid());
	print_logfile(buf);

	// Signals setup
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &signal_handler;
	sa.sa_flags = SA_RESTART;
	CHECK(sigaction(SIGWINCH, &sa, NULL)); // sigaction useful in case of a screen resize

	// SOCKET CONNECTION
	// getting port number
	if (argc < 2)
	{
		fprintf(stderr, "ERROR, no port provided\n");
		exit(0);
	}
	int portno = atoi(argv[1]);
	struct sockaddr_in serv_addr;										 // struct that describes an Internet socket address
	sockfd = CHECK(socket(AF_INET, SOCK_STREAM, 0)); // creating a new socket.
	print_logfile("New socket created\n");
	// Initializing the serv_addr struct
	bzero((char *)&serv_addr, sizeof(serv_addr)); // Sets all arguments of serv_addr to zero
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	CHECK(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))); // opening a connection on socket 'sockfd'
	print_logfile("Succesful connection to the master process\n");

	// SCREEN SETUP
	initscr();			// Initializing the console screen.
	refresh();			// refreshing the console
	clear();				// clear the console
	setup_colors(); // defining colors used in the console
	setup_map();

	int response = 0; // master's response (0:negative, 1:positive)
	while (isActive)
	{

		if (battery <= 0)
		{
			print_logfile("The battery is over, start recharging..\n");
			recharging(); // recharges the battery of the drone
			print_logfile("The battery has been fully charged!\n");
		}

		next_pos();														// receiving the next desired drone position
		next_position.timestamp = time(NULL); // takes the current time and puts it in one of the fields of drone_position struct

		CHECK(write(sockfd, &next_position, sizeof(drone_position))); // writing the next position on the socket
		print_logfile("Next position sent to the master\n");
		CHECK(read(sockfd, &response, sizeof(int))); // receiving the feedback from the master: 0--> drone cannot move, 1-->drone can move
		print_logfile("Feedback from master received\n");
		if (response == 0) // it means that another drone is trying to fly over the same position
		{
			attron(COLOR_PAIR(2));
			mvaddstr(50, 0, "The response of the master is negative: the drone cannot move =(");
			attroff(COLOR_PAIR(2));
			print_logfile("The response of the master is negative: the drone cannot move\n");
		}
		else // the drone can freely fly
		{

			attron(COLOR_PAIR(3));
			mvaddstr(50, 0, "The response of the master is positive: the drone can move =)   ");
			attroff(COLOR_PAIR(3));
			print_logfile("The response of the master is positive: the drone can move\n");
			attron(COLOR_PAIR(2));
			mvaddch(7 + actual_position.y, actual_position.x, '*'); // overwrite the current visited spot with a different colour (if it has been unexplored yet)
			attroff(COLOR_PAIR(2));
			actual_position = next_position;
			map[actual_position.y][actual_position.x] = true;
			attron(COLOR_PAIR(10));
			mvaddch(7 + actual_position.y, actual_position.x, '*'); // overwrite the current visited spot with a different colour (if it has been unexplored yet)
			attroff(COLOR_PAIR(10));
			refresh();
			sprintf(buf, "POSITION: (%d,%d)			", actual_position.y, actual_position.x);
			mvaddstr(47, 0, buf);
		}
		attron(A_BOLD);
		mvaddstr(48, 0, "BATTERY LEVEL: ");
		sprintf(buf, " %d out of 100			", battery);

		mvaddstr(48, 14, buf);
		attron(A_BOLD);

		set_speed();				// check if a new speed command has been pressed
		usleep(time_speed); // managing this we're able to modify the speed of the drone
		battery--;
	}
	close(sockfd);
	fclose(logfile); // close the logfile
	return 0;
}

void setup_map()
{
	attron(COLOR_PAIR(6));
	attron(A_BOLD);
	printw("		 _____ _    _ ____    ____  ____   ___  _   _ _____ 		\n");
	printw("		|  ___/ \\  ( ) ___|  |  _ \\|  _ \\ / _ \\| \\ | | ____|		\n");
	printw("		| |_ / _ \\ |/\\___ \\  | | | | |_) | | | |  \\| |  _|  		\n");
	printw("		|  _/ ___ \\   ___) | | |_| |  _ <| |_| | |\\  | |___ 		\n");
	printw("		|_|/_/   \\_\\ |____/  |____/|_| \\_ \\___/|_| \\_|_____|		\n");
	printw("                                                            			\n");
	attroff(A_BOLD);
	attroff(COLOR_PAIR(6));
	attron(COLOR_PAIR(7));
	printw("                                                            			");
	attroff(COLOR_PAIR(7));
	for (int i = 0; i < 40; i++) // creates a 40x80 matrix to represent the area in which the drone will be able to fly
	{
		for (int j = 0; j < 80; j++)
		{

			if (map[i][j] == false)
			{
				attron(COLOR_PAIR(1)); // foreground and background colors are both set to green, so it will generate a totally green map
				mvaddch(i + 7, j, '0');
				attroff(COLOR_PAIR(1));
			}
			else
			{
			attron(COLOR_PAIR(2)); // foreground and background colors are both set to green, so it will generate a totally green map
				mvaddch(i + 7, j, '*');
				attroff(COLOR_PAIR(2));
			}
					}
	}
	attron(A_BOLD);

	mvaddstr(49, 0, "SPEED:			");
	attron(COLOR_PAIR(4));
	mvaddstr(49, 7, "MID");
	attroff(COLOR_PAIR(4));
	 
	mvaddstr(51, 0, "SPEED COMMANDS: ");
	attron(COLOR_PAIR(4));
	mvaddstr(52, 0, "[1] SLOW, ");
	mvaddstr(52, 11, "[2] MID, ");
	mvaddstr(52, 21, "[3] HIGH, ");
	mvaddstr(52, 31, "[4] INSANE, ");
	mvaddstr(52, 43, "[5] ULTRA-INSANE");
	attroff(COLOR_PAIR(4));
	attron(COLOR_PAIR(5));
	mvaddstr(53, 0, "PRESS [q] TO QUIT");
	attroff(COLOR_PAIR(5));
		
	attroff(A_BOLD);
}

void setup_colors()
{ // colors using ncurses library

	if (!has_colors()) // check if the terminal is allowed to print colors
	{
		endwin();
		printf("This terminal is not allowed to print colors.\n");
		exit(1);
	}

	start_color();
	// list of all color used for the GUI
	init_pair(1, COLOR_GREEN, COLOR_GREEN);
	init_pair(2, COLOR_RED, COLOR_RED);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_YELLOW, COLOR_BLACK);
	init_pair(5, COLOR_RED, COLOR_BLACK);
	init_pair(6, COLOR_YELLOW, COLOR_BLUE);
	init_pair(7, COLOR_YELLOW, COLOR_YELLOW);
	init_pair(8, COLOR_WHITE, COLOR_RED);
	init_pair(9, COLOR_YELLOW, COLOR_RED);
	init_pair(10, COLOR_YELLOW, COLOR_YELLOW);
}

void signal_handler(int sig)
{
	// Function to handle the SIGWINCH signal. The OS send this signal in case of a screen resize

	if (sig == SIGWINCH)
	{
		// If the size of the terminal changes, clear and restart the grafic interface
		endwin();		 // close the window
		initscr();	 // Initialize the console screen
		clear();		 // clear the screen
		setup_map(); // setup the map
		refresh();	 // refresh the map
	}
	}

void print_logfile(char *str)
{ // function to print on logfile

	time_t cur_time = time(NULL); // takes the current time
	fprintf(logfile, "%.19s: %s", ctime(&cur_time), str);
	fflush(logfile);
}
void change_direction() // randomly generate the number of steps and the direction that the drone will take for that certain amount of steps
{
	steps++;
	dir_changer = steps % nsteps; // // nsteps is newly computed only when change_direction() is called
	if (dir_changer == 0)
	{
		nsteps = (13 + (rand() % 7 - rand() % 7)); // the robot will repeat the same movement for a random number of times that spans between 6 and 20

		delta_pos.x = (rand() % 2) - (rand() % 2); // for each direction (x and y), the allowed unitary steps are -1,0,+1
		delta_pos.y = (rand() % 2) - (rand() % 2);

		// if both the elementary steps are equal to the opposite of the respective previous ones, the drone would fly in same direction but backwards
		//if both the elementary steps are 0 or are equal to the opposite previous ones, the drone would respectively stay still or get back to its steps (which is not what we want)
		while ((delta_pos.x == 0 && delta_pos.y == 0) || (delta_pos.x == -prevdelta_pos.x && delta_pos.y == -prevdelta_pos.y)){
			 
		delta_pos.x = (rand() % 2) - (rand() % 2); 
		delta_pos.y = (rand() % 2) - (rand() % 2);
	
	}}
}

void seek_free_pos()
{

	tmp_x = actual_position.x + delta_pos.x;
	tmp_y = actual_position.y + delta_pos.y;

	if (map[tmp_y][tmp_x] == true)
	{

		if (delta_pos.y == -1 && delta_pos.x == -1)
		{

			if (map[tmp_y + 1][tmp_x] == false)
			{
				delta_pos.y += 1;
			}
			else if (map[tmp_y][tmp_x + 1] == false)
			{
				delta_pos.x += 1;
			}
		}
		else if (delta_pos.y == -1 && delta_pos.x == 0)
		{

			if (map[tmp_y][tmp_x - 1] == false)
			{
				delta_pos.x -= 1;
			}
			else if (map[tmp_y][tmp_x + 1] == false)
			{
				delta_pos.x += 1;
			}
		}
		else if (delta_pos.y == -1 && delta_pos.x == 1)
		{

			if (map[tmp_y + 1][tmp_x] == false)
			{
				delta_pos.y += 1;
			}
			else if (map[tmp_y][tmp_x - 1] == false)
			{
				delta_pos.x -= 1;
			}
		}
		else if (delta_pos.y == 0 && delta_pos.x == -1)
		{

			if (map[tmp_y - 1][tmp_x] == false)
			{
				delta_pos.y -= 1;
			}
			else if (map[tmp_y + 1][tmp_x] == false)
			{
				delta_pos.y += 1;
			}
		}
		else if (delta_pos.y == 0 && delta_pos.x == 1)
		{

			if (map[tmp_y - 1][tmp_x] == false)
			{
				delta_pos.y -= 1;
			}
			else if (map[tmp_y + 1][tmp_x] == false)
			{
				delta_pos.y += 1;
			}
		}
		else if (delta_pos.y == 1 && delta_pos.x == -1)
		{

			if (map[tmp_y - 1][tmp_x] == false)
			{
				delta_pos.y -= 1;
			}
			else if (map[tmp_y][tmp_x + 1] == false)
			{
				delta_pos.x += 1;
			}
		}
		else if (delta_pos.y == 1 && delta_pos.x == 0)
		{

			if (map[tmp_y][tmp_x - 1] == false)
			{
				delta_pos.x -= 1;
			}
			else if (map[tmp_y][tmp_x + 1] == false)
			{
				delta_pos.x += 1;
			}
		}
		else if (delta_pos.y == 1 && delta_pos.x == 1)
		{

			if (map[tmp_y - 1][tmp_x] == false)
			{
				delta_pos.y -= 1;
			}
			else if (map[tmp_y][tmp_x - 1] == false)
			{
				delta_pos.x -= 1;
			}
		}
	}
}

void next_pos()
{
	seek_free_pos();
	change_direction();

	// checking if the drone is located close to a border
	if (actual_position.x == MAX_X - 1 && delta_pos.x == 1) // right bound reached
		delta_pos.x *= -1;																		// invert step direction

	if (actual_position.x == 0 && delta_pos.x == -1) // left bound reached
		delta_pos.x *= -1;														 // invert step direction

	if (actual_position.y == MAX_Y - 1 && delta_pos.y == 1) // upper bound reached
		delta_pos.y *= -1;																		// invert step direction

	if (actual_position.y == 0 && delta_pos.y == -1) // lower bound reached
		delta_pos.y *= -1;														 // invert step direction

	// save the current unitary steps
	prevdelta_pos.x = delta_pos.x;
	prevdelta_pos.y = delta_pos.y;
	// the next position is simply a linear combination between the actual x or y coordinate and the respective elementary step along that direction
	next_position.x = actual_position.x + delta_pos.x;
	next_position.y = actual_position.y + delta_pos.y;
}

void set_speed()
{
	char des_speed;
	// Update of the whole set of file descriptors for the select() function.
	FD_ZERO(&rset);
	FD_SET(0, &rset);

	// If any of the FD of the set are "ready" (the memory of the buffer of the fifo is full)
	// the function select will return the number of FD actually "ready".
	// If no FDs are ready the function will wait until the time-out is over and it will return 0.
	ret = select(FD_SETSIZE, &rset, NULL, NULL, &tv);
	fflush(stdout);

	// If the return is negative some error possibly happen using the select() syscall.
	if (ret == -1)
	{
		perror("select()");
	}
	// If the return is greater than 0 then the fifo's buffer is ready and the process
	// can read from the pipe.
	else if (ret >= 1)
	{

		// If the FD: stdin is ready the process will assign the data contained in the buffer to
		// the variable button.
		if (FD_ISSET(0, &rset) > 0)
		{
			CHECK(read(0, &des_speed, sizeof(char)));
			attron(A_BOLD);
			switch (des_speed) // des_speed contains the speed command that the user has pressed
			{
			case '1':

				attron(COLOR_PAIR(3));
				mvaddstr(49, 7, "SLOW				");
				attroff(COLOR_PAIR(3));
				time_speed = 200000;
				print_logfile("Speed changed to: SLOW\n");
				break;
			case '2':

				attron(COLOR_PAIR(4));
				mvaddstr(49, 7, "MID				");
				attroff(COLOR_PAIR(4));
				time_speed = 100000;
				print_logfile("Speed changed to: MID\n");
				break;
			case '3':
				attron(COLOR_PAIR(5));
				mvaddstr(49, 7, "HIGH				");
				attroff(COLOR_PAIR(5));
				time_speed = 50000;
				print_logfile("Speed changed to: HIGH\n");
				break;
			case '4':
				attron(COLOR_PAIR(8));
				mvaddstr(49, 7, "INSANE");
				attroff(COLOR_PAIR(8));
				mvaddstr(49, 13, "			");
				time_speed = 20000;
				print_logfile("Speed changed to: INSANE\n");
				break;
			case '5':
				attron(COLOR_PAIR(9));
				mvaddstr(49, 7, "ULTRA-INSANE");
				attroff(COLOR_PAIR(9));
				time_speed = 2000;
				print_logfile("Speed changed to: MORE INSANE\n");
				break;
			case 'q':
				
				isActive=false;
				print_logfile("Drone quitted the simulation\n");
				
				break;
			}
			attroff(A_BOLD);
		}
	}
}

void recharging()
{
	actual_position.status = STATUS_IDLE;
	actual_position.timestamp = time(NULL);													// takes the current time and puts it in one of the fields of drone_position struct
	CHECK(write(sockfd, &actual_position, sizeof(drone_position))); // communicates to the master that the drone is recharging ('landed' status)
	attron(A_BOLD);
	attron(COLOR_PAIR(3));
	mvaddstr(48, 30, "(RECHARGING)");
	while (battery <= 100)
	{
		sprintf(buf, " %d out of 100", battery);

		mvaddstr(48, 14, buf);

		refresh();

		battery++; // charging the battery
		usleep(20000);
	}
	attroff(COLOR_PAIR(3));
	attroff(A_BOLD);
}
