FOLDER="$(find -iname src -type d)"

if [ -n "${FOLDER}" ]
then
    more -d ./../project_description.txt
    more -d ./src/dpm403_master/description.txt 
    more -d ./src/al9_3_drone/al9.txt 
    more -d ./src/FE007_3_drone/README.txt 
    more -d ./src/FA00_3_drone/report_fa_drone.txt 
    more -d ./src/ML99_3_drone/report.txt
else 
    more -d project_description.txt
fi