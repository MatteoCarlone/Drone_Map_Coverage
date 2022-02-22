if [ $# -eq 0 ]		#A <pathname> is required
then
	echo "Usage: ./install.sh <pathname> "; 
	exit;
fi

if [ ! -d $1 ] #if the directory doesn't exist make the desired directory
then
	echo "Directory $1 DOES NOT exist.";
	while true; do
		read -p "Do you wish to create $1 directory? [Y/n] " yn
		case $yn in
			[Y]* ) mkdir $1; break;;	#make the directory
			[n]* ) exit;;
			* ) "Please, press Y for yes or n for no.";;
		esac
	done
fi

# echo "Begin program installation on $1 ... ";


unzip ./src.zip;	#Unzip the src folder
mv ./src $1;		#move the unzipped src folder in the <pathname> directiory
cp ./help.sh $1;	#copy the help bash in the <pathname> directiory
cp run.sh $1;		#copy the run bash in the <pathname> directiory


# echo "Program installed on $1";

dpkg --status konsole &> /dev/null 	#Checking if the konsole pkg is already installed
if [ $? -eq 0 ]
then
	echo "konsole pkg already installed."
else										#If the pkg is not installed it will be installed.
	echo "For compiling, konsole pkg is needed.";
	while true; do
		read -p "Do you wish to continue? [Y/n] " yn #installing the konsole pkg for running
		case $yn in
			[Y]* ) sudo apt-get install -y konsole; break;; # You need to be superuser!
			[n]* ) exit;;
			* ) "Please, answer Y for yes or n for no.";;
		esac
	done
fi

dpkg --status libncurses-dev &> /dev/null 	#Checking if the ncurses library is already installed.
if [ $? -eq 0 ]
then
	echo "ncurses library already installed."
else										#If the library is not installed it will be installed
	echo "For compiling, ncurses library is needed.";
	while true; do
		read -p "Do you wish to continue? [Y/n] " yn #installing the ncurses library that is needed for the Graphic Interface
		case $yn in
			[Y]* ) sudo apt-get install -y libncurses-dev; break;; # You need to be superuser!
			[n]* ) exit;;
			* ) "Please, answer Y for yes or n for no.";;
		esac
	done
fi

echo "Begin sources' compilation ...";

# Compiles all scripts.
# The <pathname> directory will contain the src folder, the help.sh and run.sh bash scripts and a README.txt
# All executables will be in their own respective directories in the src folder.

gcc $1/src/dpm403_master/master.c -lncurses -lm -o $1/src/dpm403_master/master;
gcc $1/src/FE007_3_drone/drone.c -lncurses -lm -o $1/src/FE007_3_drone/drone;
gcc $1/src/FA00_3_drone/drone.c -lncurses -lm -o $1/src/FA00_3_drone/drone;
gcc $1/src/ML99_3_drone/drone.c -lncurses -lm -o $1/src/ML99_3_drone/drone;
gcc $1/src/al9_3_drone/drone.c -lncurses -lm -o $1/src/al9_3_drone/drone;

echo "You can run the program in $1 with ./run.sh ";
