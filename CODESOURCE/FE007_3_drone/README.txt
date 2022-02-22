


##### DRONE FE007 #####

This program represents a drone flying simulation. 

The drone can move into a field of 80 x 40 meters, and it has a battery that drains while flying. When the drone runs out of battery, it lands and starts recharging. 
Moreover, you can set the drone's velocity by selecting the following commands:

	[1] SLOW
	[2] DEFAULT
	[3] FAST
	[3] VERY FAST

Obviously, the faster the drone is, the quicker the battery will drain!

The program prints on the terminal some helpful information, such as the battery state, the drone visited/non-visited positions, and the feedback received by the master process. Therefore a graphical tool with the 'ncurses' library has been implemented as GUI.
Runtime it modifies and shows a 80x40 bool matrix: 
	
	- Green '0' -> 	Positions not visited yet
	- Red 	'1' -> 	Visited positions

The drone chooses where to go based on the results of a coverage algorithm. Indeed, this is the algorithm idea: first, it increments its 'x' coordinate by one and randomly the 'y' in a -1, +1 range, remaking the choice if the position is one of the already visited positions.
When the drone runs out of battery, it stops, it starts recharging, and 
finally, it changes the non-random visiting direction, moving on the 'y' coordinate, and so on.
Therefore, the drone chooses randomly between the three frontal positions, looping this choice ten times to find a not yet visited one. If all frontal positions are already visited, the drone chooses one randomly and continues visiting the map.
This algorithm does not ensure to cover the map and is not an efficient one because it sometimes reviews the same positions, especially when a big part of it has already been covered. 
However, the performed random choice is necessary to avoid the drone getting stuck in front of an obstacle or another drone on the map.

Finally, the drone movement relies on the master process that  
acts as a server for all the drones that connect with the master 
the process through a socket. 
In particular, the communication protocol has been established among all the assignment participants, and the result is that all the drones correctly move into the 80x40 field without colliding with each other.