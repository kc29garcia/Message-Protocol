README

This project is coded in C and was created in a Linux enviornment. In clientFinal I hard coded the IP address for the server so to get the server IP address in Linux: 

hostname -I 
and first IP address is the server IP address.

compile server: gcc -o serverFinal serverFinal.c -lpthread
compile client: gcc -o clientFinal clientFinal.c -lpthread

run server first: ./serverFinal
run client second: ./clientFinal

first go to server and get the client id and to send a message to client enter:
<client_id> <message to be sent>

use the first client ID that is outputted everytime a message needs to be sent to the user.

on client side just type a message and hit enter.