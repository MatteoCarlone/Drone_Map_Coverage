// Including the drone.h header file.
// Compiling with gcc drone.c -lm -lncurses -o drone

#include "drone.h"

int main(int argc, char * argv[]){
    int level;

    // The following instrucion initializes the creation of random numbers which 
    // will be used by rand() in order to get new different random values each execution.

    srand(time(0));

    // Declaring the file descriptor of the socket, the port number and the
    // address of the server and of the client.

    int sockfd;                            
    int portno = atoi(argv[1]);                      
    struct sockaddr_in serv_addr;     

    // Creates and writing PID on the logfile.

    logfile = fopen("logfile/log_drone_ML99.txt", "w");
    LogPrint("Creating the debug file, process starts.\n");
    sprintf(str, "The PID of the process is: %d \n", getpid());
    LogPrint(str);

    // Implementing the structure of the signals.

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = SA_RESTART;
    
    // Sigaction for SIGWINCH, when the window resizes, the system sends a signal to the process
    // when the size of the window resizes. 

    CHECK(sigaction(SIGWINCH,&sa,NULL));

    /*
    Creating a new socket using the function socket():
        Arguments:
            * AF_INET: to specify the internet domain of the address
            * SOCK_STREAM: the type of the socket, in this case the charachters will be read in a continuous stream
            * The protocol, if zero the operating system will choose the most appropriate protocol. 
              In this case TCP for stream sockets
        Returns:
            * sockfd: file descriptor
    */

    sockfd = CHECK(socket(AF_INET, SOCK_STREAM, 0));

    /*
    Setting fields of the struct serv_addr:
        * sin_family: contains a code for the address family 
        * sin_addr.s_addr: This field contains the IP address of the host. 
                           For server code, this will always be the IP address of the machine on which the server
                           is running. To get we use the symbolic constant INADDR_ANY.
        *sin_port: contain the port number, htons() convert the port number in host byte order to a port number
                    in network byte order.
    */

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // The connect function is called by the client to establish a connection to the server.

    CHECK(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)));

    // If all went okay, the connection was estabilished.

    LogPrint("Connection succesfully established.\n");

    // Setting up the console with the map.

    initscr();
    refresh();
    clear();
    SetupColors();
    SetupMap();
    
    // Filling the matrix grid with 0s, in order to start the exploration
    // of the map.

    for (int i = 0; i < 40; i++) {
        for (int j = 0; j < 80; j++) grid[i][j] = 0;
    }
    
    // Looping the program to start the task of the drone.

    while (current_velocity!=0){

        // If the battery is down, we have to recharge it so we land the drone
        // and start the recharging routine, when it's not recharging, we ref-
        // resh the window to always assure that the window is displaying the
        // right values.

        if (battery == 0) RechargingBattery(sockfd);
        else {
            SetupMap();
            refresh();
        }

        // When the origin is reached, the drone starts looking for a new pos-
        // ition, obviously not already visited. When the goal has been reach-
        // ed, the process toggles the bool value in order to get a new rando-
        // mized position.

        if(originreached == true){
            if(goalreached == true) NewRandPosition();
            NewPositionUpdate();
        }
        else {
            GoToOrigin();
        }

        // Writing the next position to the master process. 
        // Printing all infos to the logfile.

        new_position.timestamp = time(NULL);
        CHECK(write(sockfd, &new_position, sizeof(drone_position)));
        LogPrint("Data correctly written to the socket client.\n");

        // Reads a feedback. The command is 0 if it's not allowed and
        // it is 1 if it is allowed.
        // Printing all infos to the logfile.

        CHECK(read(sockfd, &canmove, sizeof(int)));
        LogPrint("Feedback correctly read from the socket. \n");

        // If we cannot move, we stop and wait instructions from the master.

        if (canmove == 0)
        {
            attron(COLOR_PAIR(2));
            mvaddstr(47, 0, "Drone stopped, position not allowed!              ");
            attroff(COLOR_PAIR(2));
            refresh();
            LogPrint("Drone stopped, position not allowed, changing aim. \n");
            NewRandPosition();
        }

        // If we can move, then we procede with the movement and go on
        // of one step.

        if (canmove == 1)
        {

            // Printing on console and on logfile.

            attron(COLOR_PAIR(3));
            mvaddstr(47, 0, "Position allowed! The drone is making its steps...");
            attroff(COLOR_PAIR(3));
            refresh();
            LogPrint("Position allowed, drone moving. \n");

            // Leaving a black dot behind and moving all the propellers of the drone
            // in the direction asked by the position function.

            PRINT(5, actual_position.y+1, actual_position.x+1, '1');

            if(grid[actual_position.y -1][actual_position.x + 1] == 1){
                PRINT(5, actual_position.y, actual_position.x+2, '1');
            }
            else if(grid[actual_position.y -1][actual_position.x + 1] == 0){
                PRINT(6, actual_position.y , actual_position.x + 2, '0');
            }

            if(grid[actual_position.y -1][actual_position.x -1] == 1){
                PRINT(5, actual_position.y  , actual_position.x , '1');
            }else if(grid[actual_position.y -1][actual_position.x -1] == 0){
                PRINT(6, actual_position.y , actual_position.x , '0');
            }

            if(grid[actual_position.y +1][actual_position.x + 1] == 1){
                PRINT(5, actual_position.y +2 , actual_position.x + 2, '1');
            }else if(grid[actual_position.y +1][actual_position.x + 1] == 0){
                PRINT(6, actual_position.y +2 , actual_position.x + 2, '0');
            }
            
            if(grid[actual_position.y +1][actual_position.x - 1] == 1){
                PRINT(5, actual_position.y +2 , actual_position.x , '1');
            }else if(grid[actual_position.y +1][actual_position.x - 1] == 0){
                PRINT(6, actual_position.y +2 , actual_position.x, '0');
            }

            refresh();

            actual_position = new_position;
            grid[actual_position.y][actual_position.x] = 1;

            PRINT(10, actual_position.y + 1, actual_position.x + 1, '#');
            PRINT(10, actual_position.y , actual_position.x + 2, 'X');
            PRINT(10, actual_position.y , actual_position.x , 'X');
            PRINT(10, actual_position.y + 2, actual_position.x +2 , 'X');
            PRINT(10, actual_position.y + 2, actual_position.x, 'X');

            refresh();

        }

        refresh();

        // Battery decreasing and displaying it using the LoadingBattery() function.

        battery--;
        LoadingBattery(battery, MAX_CHARGE);
        current_velocity = increase_velocity(current_velocity);
        if (current_velocity == 200000) level = 1;
        if (current_velocity == 100000) level = 2;
        if (current_velocity == 50000) level = 3;
        if (current_velocity == 25000) level = 4;
        attron(COLOR_PAIR(2));
        sprintf(str,"Your currenty velocity level is: %d!", level);

        mvaddstr(45, 53, str);
        attroff(COLOR_PAIR(2));

        usleep(current_velocity);

        // Printing the current position and the one which the robot aims to.

        attron(COLOR_PAIR(3));
        sprintf(str,"Actual position %d x %d y  ", actual_position.x, actual_position.y);
        mvaddstr(46, 53, str);
        sprintf(str,"Desired position %d x %d y  ", desired_position.x, desired_position.y);
        mvaddstr(47, 53, str);
        attroff(COLOR_PAIR(3));
        refresh();
    }

    CHECK(fclose(logfile));

    CHECK(close(sockfd));

    return 0;
}
