/**
 * @file
 * @brief This file implements the storage server.
 *
 * The storage server should be named "server" and should take a single
 * command line argument that refers to the configuration file.
 * 
 * The storage server should be able to communicate with the client
 * library functions declared in storage.h and implemented in storage.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "utils.h"
#include "storage.h"

extern FILE *yyin;
unsigned int botRT, topRT;
ThreadInfo runtimeThreads[MAX_CONNECTIONS];

/* Mutex to guard SET statements */
pthread_mutex_t setMutex;

/* Mutex to guard print statements */ 
pthread_mutex_t  printMutex; 

/* Mutex to guard condition -- used in getting/returning from thread pool*/ 
pthread_mutex_t conditionMutex;
/* Condition variable -- used to wait for avaiable threads, and signal when available */ 
pthread_cond_t  conditionCond;

#define MAX_LISTENQUEUELEN 20	///< The maximum number of queued connections.

#define LOGGING 1 //Server-side logging

int flag = 0;
/**
 * @brief Process a command from the client.
 *
 * @param sock The socket connected to the client.
 * @param cmd The command received from the client.
 * @return Returns 0 on success, -1 otherwise.
 */

/*** New Protocol Prototype ***/
//The server parses line received from the buffer and puts them into a list
//e.g. "name Bloor Danforth , kilometres 26 , stops 31"
//will be parsed into:
// "@name@$Bloor Danforth$!@kilometres@#26#!@stops@#31#?"
//&<command type>&
//^<table/username>^
//*<key>*
// @<typename>@
// $strval$
// #intval#
// <prevcolumn>!<nextcolumn>
// '?' signifies the end of input
/*** End of Protocol Prototype ***/
int handle_command(int sock, char *cmd, FILE *fptr, struct config_params *params, struct city **headlist, int *auth_success)
{
    int counter;
    char commandname[MAXLEN];//also used for return:command
    char tablename[MAXLEN];//also used for return:status
    char keyname[MAXLEN];
    char valuename[MAXLEN];//also used for return:secondstatus (may be detail OR value)
    char retline[MAXLEN];
    char encoded_value[MAXLEN];
    //char returncmd[MAXLEN+1]; //To send back to client
    time_t rawtime;
    struct tm * timeinfo;
    int index = 0;
    int tempcmd = 1;
    int tempcommand = 0;
    int i = 0;//common-purpose counter
    printf("command received: %s\n", cmd);
    while(cmd[tempcmd] != '&'){
	if(cmd[tempcmd] != '&'){
	    commandname[tempcommand] = cmd[tempcmd];
	    tempcommand++;
	    tempcmd++;
	}
    }
    commandname[tempcommand] = '\0';
    tempcommand = 0;
    tempcmd++;
    tempcmd++;
    while(cmd[tempcmd] != '^'){
	tablename[tempcommand] = cmd[tempcmd];
	tempcommand++;
	tempcmd++;
    }
    tablename[tempcommand] = '\0';
    tempcommand = 0;
    tempcmd++;
    while(cmd[tempcmd] != '\0'){
	valuename[tempcommand] = cmd[tempcmd];
	tempcommand++;
	tempcmd++;
    }
    valuename[tempcommand] = '\0';
    tempcommand = 0;
    if(strcmp(commandname, "QUERY") == 0){
	printf("command is: %s\n", commandname);
	printf("table is: %s\n", tablename);
	printf("valuename: %s\n", valuename);
    }
    else {
	decode_line(cmd, commandname, tablename, keyname, valuename, &counter);
    }
    //printf("commandname: %s\n", commandname);
    //printf("tablename: %s\n", tablename);
    //printf("keyname: %s\n", keyname);
    //printf("valuename: %s\n", valuename);
    char namegen[MAXLEN+1];
    if(LOGGING==2){
	char tempstr[MAXLEN+1];
	time(&rawtime);
	timeinfo=localtime(&rawtime);
	sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	//sprintf(tempstr,"Processing command '%s'\n",commandname);
	printf("Processing line \"%s\"\n", cmd);
	logger(fptr,namegen);//Timestamp
	logger(fptr,tempstr);
    }
    else if(LOGGING==1){
	time(&rawtime);
	timeinfo=localtime(&rawtime);
	sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	printf("%s",namegen);//Timestamp
	//printf("Processing command '%s'\n",commandname);
	printf("Processing line \"%s\"\n", cmd);
    }
    
    if (strcmp(commandname, "SET") == 0){
	
    	pthread_mutex_lock( &setMutex );
    	
	//SET below
	// check if authorized
	if ((*auth_success) == 0){
	    //printf("inside if statement.\n");
	    //strncpy(success, "AUTH", sizeof(success));
	    //sendall(sock, success, sizeof(success));
	    cleanstring(tablename);
	    cleanstring(valuename);
	    sprintf(tablename, "FAIL");
	    sprintf(valuename, "AUTH");
	    encode_line(commandname, tablename, valuename, retline);
	    sendall(sock, retline, sizeof(retline));
	}
	else {
	    index = find_index(params->tablelist, tablename);
	    if(index != -1){
		//name found
		struct city *head = headlist[index];
		struct city *temp = find_city(head, keyname);
		printf("valuename: %s\n", valuename);
		if(temp == NULL){
		    //entry doesn't exist
		    if(strcmp(valuename, "@NULL@?") == 0){
			//deleting a key that doesn't exist
			//sprintf(success, "SET$KEY");
			//sendall(sock, success, sizeof(success));
			cleanstring(tablename);
			cleanstring(valuename);
			sprintf(tablename, "FAIL");
			sprintf(valuename, "KEY");
			encode_line(commandname, tablename, valuename, retline);
			sendall(sock, retline, sizeof(retline));
		    }
		    else{
			int column_count = 0;
			column_count = count_column(valuename);		
			if (params->num_columns[index] == column_count)
			    {
				insert_city(headlist[index], keyname, valuename);
				//VALUE IS INSERTED AS A STRING
				cleanstring(tablename);
				cleanstring(valuename);
				sprintf(tablename, "SUCCESS");
				sprintf(valuename, "CREATE");
				encode_line(commandname, tablename, valuename, retline);
				sendall(sock, retline, sizeof(retline));
			    }
			else
			    {
				sprintf(retline, "SET FAIL COLUMN\n");
				sendall(sock, retline, sizeof(retline));						
			    }
		    }
		}
		else {
		    //entry exists, temp != NULL
		    if(strcmp(valuename, "@NULL@?") == 0){
			//delete
			//printf("delete entry\n");
			delete_city(&head, keyname);
			temp = NULL;
			//sprintf(success, "SET$SUCCESS");
			//sendall(sock, success, sizeof(success));
			cleanstring(tablename);
			cleanstring(valuename);
			sprintf(tablename, "SUCCESS");
			sprintf(valuename, "DELETE");
			encode_line(commandname, tablename, valuename, retline);
			sendall(sock, retline, sizeof(retline));
		    }
		    else{
			//modify record
			//strncpy(temp->population, , sizeof(temp->population));
			//sprintf(success, "SET$SUCCESS");
			//sendall(sock, success, sizeof(success));
			//printf("valuename: %s\n", valuename);
			if (counter == temp->counter || counter == 0)
			    {
				int column_count = 0;
				temp = find_city(headlist[index], keyname);
				column_count = count_column(valuename);
				
				if (temp->numocolumns == column_count)
				    {
					modify_city(temp, valuename);
					cleanstring(tablename);
					cleanstring(valuename);
					sprintf(tablename, "SUCCESS");
					sprintf(valuename, "MODIFY");
					encode_line(commandname, tablename, valuename, retline);
					sendall(sock, retline, sizeof(retline));							
				    }
				else
				    {
					sprintf(retline, "SET FAIL COLUMN\n");
					sendall(sock, retline, sizeof(retline));						
				    }
			    }
			else
			    {
				char temp[100];
				sprintf(temp, "%s", "SET FAIL COUNTER");
				sendall(sock, temp, sizeof(temp));	
			    }
		    }
		}
	    }
	    else {
		//table doesn't exist
		//strncpy(fail, "SET$TABLE", sizeof(fail));
		//sendall(sock, fail, sizeof(fail));
		cleanstring(tablename);
		cleanstring(valuename);
		sprintf(tablename, "FAIL");
		sprintf(valuename, "TABLE");
			encode_line(commandname, tablename, valuename, retline);
			sendall(sock, retline, sizeof(retline));
	    }
	}
	
	pthread_mutex_unlock( &setMutex );
    }
    
    // For now, just send back the command to the client.
    //command cases: get, set
    
    else if(strcmp(commandname, "AUTH") == 0){
	//AUTH below
	if(strcmp(tablename, params->username) == 0){
	    if (strcmp(keyname, params->password) == 0){
		(*auth_success) = 1;
		printf("authenticated\n");
		//strncpy(success, "AUTH$SUCCESS", sizeof(success));
		//sendall(sock, success, sizeof(success));
		cleanstring(tablename);
		cleanstring(valuename);
		sprintf(tablename, "SUCCESS");
		sprintf(valuename, " ");
		encode_line(commandname, tablename, valuename, retline);
		sendall(sock, retline, sizeof(retline));
	    }
	    else {
		printf("authenticated\n");
		cleanstring(tablename);
		cleanstring(valuename);
		sprintf(tablename, "FAIL");
		sprintf(valuename, " ");
		encode_line(commandname, tablename, valuename, retline);
		sendall(sock, retline, sizeof(retline));
		//strncpy(fail, "AUTH$FAIL", sizeof(fail));
		//sendall(sock, fail, sizeof(fail));
			}
	}
	else {
	    cleanstring(tablename);
	    cleanstring(valuename);
	    sprintf(tablename, "FAIL");
	    sprintf(valuename, "");
	    encode_line(commandname, tablename, valuename, retline);
	    sendall(sock, retline, sizeof(retline));
	    //strncpy(fail, "AUTH$FAIL", sizeof(fail));
	    //sendall(sock, fail, sizeof(fail));
	}
    }
    else if(strcmp(commandname, "GET") == 0){
	// check if authenticated
	if ((*auth_success) == 0){
	    //not authenticated
	    cleanstring(tablename);
	    cleanstring(valuename);
	    sprintf(tablename, "FAIL");
	    sprintf(valuename, "AUTH");
	    encode_line(commandname, tablename, valuename, retline);
	    sendall(sock, retline, sizeof(retline));
	    //strncpy(success, "AUTH", sizeof(success));
	    //sendall(sock, success, sizeof(success));
	}
	printf("tablename: %s\n", tablename);
	index=find_index(params->tablelist, tablename);
	printf("index: %d\n", index);
	if(index != -1) {
	    //name found
	    struct city *head = (headlist)[index];
	    print_city(head);
	    struct city *temp = find_city(head, keyname);
	    //printf("temp key: %s\n", temp->name);
	    //printf("temp colnum: %d\n", temp->numocolumns);
	    if(temp == NULL){
		//key doesn't exist
		//strncpy(fail, "GET$KEY$FAIL$FAIL", sizeof(fail));
		//sendall(sock, fail, sizeof(fail))
		
		cleanstring(tablename);
		cleanstring(valuename);
		sprintf(tablename, "FAIL");
		sprintf(valuename, "KEY");
		//printf("key doesn't exist\n");
		encode_line(commandname, tablename, valuename, retline);
		sendall(sock, retline, sizeof(retline));
		
	    }
	    else {
		//key exists
		//printf("columnlist: %s\n", temp->columnlist);
		//printf("encoded_value: %s\n", encoded_value);
		//printf("temp->numocolumns: %d\n", temp->numocolumns);
		cleanstring(encoded_value);
		encode_retval(temp->columnlist, encoded_value, temp->numocolumns);
		cleanstring(tablename);
		cleanstring(valuename);
		sprintf(tablename, "SUCCESS");
		sprintf(valuename, "%s", encoded_value);
		printf("serverside\n");
		printf("tablename: %s\n", tablename);
		printf("valuename: %s\n", valuename);
		encode_line(commandname, tablename, valuename, retline);

		
		char temp_str[100];
		char number[100];
		strcpy(temp_str, " COUNTER ");
		sprintf(number, "%d", temp->counter);
		strcat(temp_str, number);
		strcat(retline, temp_str);
		sendall(sock, retline, sizeof(retline));
		//sprintf(success, "GET$SUCCESS$%s$%s", temp->name, temp->population);
		//sendall(sock, success, sizeof(success));
	    }
	}
	else {
	    //table doesn't exist
	    cleanstring(tablename);
	    cleanstring(valuename);
	    sprintf(tablename, "FAIL");
	    sprintf(valuename, "TABLE");
	    encode_line(commandname, tablename, valuename, retline);
	    sendall(sock, retline, sizeof(retline));
	    //strncpy(fail, "GET$TABLE$FAIL$FAIL", sizeof(fail));
	    //sendall(sock, fail, sizeof(fail));
	}
    }
    else if(strcmp(commandname, "QUERY") == 0) {//query
	//what to do?
	//first allocate a keylist
	//then do searching
	//when done, encode the keylist and send back to client
	puts("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$HANDLEQUERY$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
	struct queryarg *testque = (struct queryarg *)malloc(sizeof(struct queryarg));
	int numque = 0;
	int questatus = 0;
	char server_keylist[1000][1024];
	for(i = 0; i < 1000; i++){
	    cleanstring(server_keylist[i]);
	}
	strncpy(server_keylist[0], "testcopy", sizeof(server_keylist[0]));
	index = find_index(params->tablelist, tablename);
	//printf("index is %d\n", index);
	if(index != -1){
	    //found matching name in tablelist
	    struct city *node = (headlist)[index];
	    numque = query_argument(testque, valuename);
	    //printf("numque = %d\n", numque);
	    testque->max_keys++;
	    questatus = query_write(server_keylist, testque, node, &testque->max_keys, &numque);
	    if(questatus == -1) {
		//printf("query incorrect\n");
	    }
	    else if(questatus == 0) {
		//printf("query correct\n");
		i = 1;
		if(server_keylist[i][0] == '\0') {
		    printf("no matching keys detected.\n");
		}
		while(server_keylist[i][0] != '\0'){
		    printf("keylist[%d]: %s\n", i, server_keylist[i]);
		    i++;
		}
		if(i > testque->max_keys){
		    i = testque->max_keys;
		}
		encode_queryret(i, server_keylist, retline);
		//printf("query retline: %s\n", retline);
		sendall(sock, retline, sizeof(retline));
	    }
	    free(testque);
	    testque = NULL;
	}
	else{
	    //table doesn't exist
	    printf("table doesn't exist\n");
	    cleanstring(retline);
	    sprintf(retline, "&QUERY&$FAIL$^TABLE^");
	    sendall(sock, retline, sizeof(retline));
	}
	puts("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%HANDLEQUERY_END%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%");
    }
    sendall(sock, "\n", 1);
    return 0;
}

void * threadCallFunction(void *arg) { 
    ThreadInfo tiInfo = (ThreadInfo)arg; 

	// Get commands from client.
	int wait_for_commands = 1;
	do {
		// Read a line from the client.
		char cmd[MAX_CMD_LEN];
		int status = recvline(tiInfo->clientsock, cmd, MAX_CMD_LEN);
		
		if (status != 0) {
			// Either an error occurred or the client closed the connection.
			wait_for_commands = 0;
		}
		else {
			// Handle the command from the client.
			int status = handle_command(tiInfo->clientsock, cmd, tiInfo->fileptr, tiInfo->params, tiInfo->headlist, &(tiInfo->auth_success));
	
			if (status != 0)
				wait_for_commands = 0; // Oops.  An error occured.
	
		}
		
	} while (wait_for_commands);  
  
  
	if (close(tiInfo->clientsock)<0) { 
	pthread_mutex_lock( &printMutex ); 
	printf("ERROR in closing socket to %s:%d.\n", 
	   inet_ntoa(tiInfo->clientaddr.sin_addr), tiInfo->clientaddr.sin_port);
	pthread_mutex_unlock( &printMutex ); 
	}
	releaseThread( tiInfo ); 


	return NULL; 
}


int main(int argc, char *argv[])
{
    //Variable declarations
    FILE *fileptr = NULL;
    char tmpstring[MAXLEN+1];
    time_t rawtime;
    struct tm * timeinfo;
    char namegen[1024];
    int k=0;
    
    int status = 0;
    yyin = fopen(argv[1], "r");
    struct config_params params;
    struct storage_record record_temp;
    struct bigstring str;
	int max_keys = 10;
	char keynames[10][100];
    yyparse(&params, &record_temp, &str, &max_keys, keynames, &status);
    
    printf("port number: %d\n", params.server_port);

    printf("status1: %d\n", status);
    if (status == -1)
    {
    	printf("Error in configration files\n");
    	errno = ERR_INVALID_PARAM;
    	exit(EXIT_FAILURE);
    }
    
    printf("status2: %d\n", status);
    
    status = check_column(&params);
    
    printf("status3: %d\n", status);
    
    if (status == -1)
    {
    	printf("Error in configration files\n");
    	errno = ERR_INVALID_PARAM;
    	exit(EXIT_FAILURE);
    }    
     
     
     
    struct city **headlist=(struct city**)malloc(sizeof(struct city*) * MAX_TABLES);
    for(k=0;k<100;k++){
	headlist[k]=(struct city*)malloc(sizeof(struct city));
    }
    //End of variable declarations
    
    if(flag!=1&&LOGGING==2){
    	time ( &rawtime );
    	timeinfo = localtime ( &rawtime );
    	sprintf(namegen,"Server-%.4d-%.2d-%.2d-%.2d-%.2d-%.2d.log",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
    	fileptr=fopen(namegen,"w");
    	flag=1;
    }
    
    // Process command line arguments.
    //This program expects exactly one argument: the config file name.
    assert(argc > 0);
    if (argc != 2){
	if(LOGGING==2){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    sprintf(tmpstring,"Usage %s <config_file>\n",argv[0]);
	    logger(fileptr,namegen);//Timestamp
	    logger(fileptr,tmpstring);
	}
	else if(LOGGING==1){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    printf("%s",namegen);//Timestamp
	}
	
	printf("Usage %s <config_file>\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    
    if (status != 0) {
	if(LOGGING==2){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    sprintf(tmpstring,"Error processing config file.\n");
	    logger(fileptr,namegen);//Timestamp
	    logger(fileptr,tmpstring);
	}
	else if(LOGGING==1){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    printf("%s",namegen);//Timestamp
	}
	
	printf("Error processing config file.\n");
	exit(EXIT_FAILURE);
    }
    
    if(LOGGING==2){
	sprintf(tmpstring,"Server on %s:%d\n",params.server_host, params.server_port);
	time(&rawtime);
	timeinfo=localtime(&rawtime);
	sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	logger(fileptr,namegen);//Timestamp
	logger(fileptr,tmpstring);
    }
    else if(LOGGING==1){
	time(&rawtime);
	timeinfo=localtime(&rawtime);
	sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	printf("%s",namegen);//Timestamp
	printf("Server on %s:%d\n",params.server_host,params.server_port);
    }
    
    
    
    // Create a socket.
    int listensock = socket(PF_INET, SOCK_STREAM, 0);
    
    if (listensock < 0) {
	if(LOGGING==2){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    sprintf(tmpstring,"Error creating socket.\n");
	    logger(fileptr,namegen);//Timestamp
	    logger(fileptr,tmpstring);
	}
	else if(LOGGING==1){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    printf("%s",namegen);//Timestamp
	}
	
	printf("Error creating socket.\n");
	exit(EXIT_FAILURE);
    }
    
    
    
    // Allow listening port to be reused if defunct.
    int yes = 1;
    status = setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    
    if (status != 0) {
	
	if(LOGGING==2){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    sprintf(tmpstring,"Error configuring socket.\n");
	    logger(fileptr,namegen);//Timestamp
	    logger(fileptr,tmpstring);
	}
	else if(LOGGING==1){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    printf("%s",namegen);//Timestamp
	}
	
	printf("Error configuring socket.\n");
	exit(EXIT_FAILURE);
    }
    
    
    
    // Bind it to the listening port.
    struct sockaddr_in listenaddr;
    memset(&listenaddr, 0, sizeof listenaddr);
    listenaddr.sin_family = AF_INET;
    listenaddr.sin_port = htons(params.server_port);
    inet_pton(AF_INET, params.server_host, &(listenaddr.sin_addr)); // bind to local IP address
    status = bind(listensock, (struct sockaddr*) &listenaddr, sizeof listenaddr);
    if (status != 0) {
	
	if(LOGGING==2){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    sprintf(tmpstring,"Error binding socket.\n");
	    logger(fileptr,namegen);//Timestamp
	    logger(fileptr,tmpstring);
	}
	else if(LOGGING==1){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    printf("%s",namegen);//Timestamp
	}
	
	printf("Error binding socket.\n");
	exit(EXIT_FAILURE);
    }
    
    
    
    // Listen for connections.
    status = listen(listensock, MAX_LISTENQUEUELEN);
    if (status != 0) {
	
	if(LOGGING==2){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    sprintf(tmpstring,"Error listening on socket.\n");
	    logger(fileptr,namegen);//Timestamp
	    logger(fileptr,tmpstring);
	}
	else if(LOGGING==1){
	    time(&rawtime);
	    timeinfo=localtime(&rawtime);
	    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	    printf("%s",namegen);//Timestamp
	}
	
	printf("Error listening on socket.\n");
	exit(EXIT_FAILURE);
    }
    
    
    
    if (params.option == 0)
	{
	    // Listen loop.
	    int wait_for_connections = 1;
	    
	    while (wait_for_connections) {
		// Wait for a connection.
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof clientaddr;
		int clientsock = accept(listensock, (struct sockaddr*)&clientaddr, &clientaddrlen);
		
		
		if (clientsock < 0) {	    
		    if(LOGGING==2){
			time(&rawtime);
			timeinfo=localtime(&rawtime);
			sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			sprintf(tmpstring,"Error accepting a connection.\n");
			logger(fileptr,namegen);//Timestamp
			logger(fileptr,tmpstring);
		    }
		    else if(LOGGING==1){
			time(&rawtime);
			timeinfo=localtime(&rawtime);
			sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			printf("%s",namegen);//Timestamp
		    }
		    
		    printf("Error accepting a connection.\n");
			exit(EXIT_FAILURE);
		}
		

		if(LOGGING==2){
		    sprintf(tmpstring,"Got a connection from %s:%d\n",inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
		    time(&rawtime);
		    timeinfo=localtime(&rawtime);
		    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		    logger(fileptr,namegen);//Timestamp
		    logger(fileptr,tmpstring);
		}
		else if(LOGGING==1){
		    time(&rawtime);
		    timeinfo=localtime(&rawtime);
		    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		    printf("%s",namegen);//Timestamp
		    printf("Got a connection from %s:%d\n",inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
		}
		
		int auth_success = 0;
		
		// Get commands from client.
		int wait_for_commands = 1;
		do {
		    // Read a line from the client.
		    char cmd[MAX_CMD_LEN];
		    int status = recvline(clientsock, cmd, MAX_CMD_LEN);
		    
		    if (status != 0) {
			// Either an error occurred or the client closed the connection.
			wait_for_commands = 0;
		    }
			else {
			    // Handle the command from the client.
			    //printf("OUTSIDE %p", headlist);
			    int status = handle_command(clientsock, cmd, fileptr, &params, headlist, &auth_success);
			    
			    if (status != 0)
				wait_for_commands = 0; // Oops.  An error occured.
			    
			}
		    
		} while (wait_for_commands);
		
		// Close the connection with the client.
		close(clientsock);
		//LOG(("Closed connection from %s:%d.\n", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port));
		if(LOGGING==2){
		    sprintf(tmpstring, "Closed connection from %s:%d\n",inet_ntoa(clientaddr.sin_addr),clientaddr.sin_port);
		    time(&rawtime);
		    timeinfo=localtime(&rawtime);
		    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		    logger(fileptr,namegen);//Timestamp
		    logger(fileptr,tmpstring);
		}
		else if(LOGGING==1){
		    time(&rawtime);
			timeinfo=localtime(&rawtime);
			sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			printf("%s",namegen);//Timestamp
			printf("Closed connection from %s:%d\n",inet_ntoa(clientaddr.sin_addr),clientaddr.sin_port);
		}
		
		auth_success = 0;
		
	    }
	    // Stop listening for connections.
	    close(listensock);
	    //free(tmpstring);
	    return EXIT_SUCCESS;    	
	}
    else if (params.option == 1)
	{
	    // Allocate threads pool
	    int i;
	    for (i = 0; i!=MAX_CONNECTIONS; ++i)
		{
		    runtimeThreads[i] = malloc( sizeof( struct _ThreadInfo ) );    
		}
	    
	    // Listen loop.
	    int wait_for_connections = 1;
	    
	    while (wait_for_connections) {
		// Wait for a connection.
		ThreadInfo tiInfo = getThreadInfo(); 
		tiInfo->clientaddrlen = sizeof(struct sockaddr_in); 		
		tiInfo->clientsock = accept(listensock, (struct sockaddr*)&tiInfo->clientaddr, &tiInfo->clientaddrlen);
		
		tiInfo->fileptr = fileptr;
		tiInfo->params = &params;
		tiInfo->headlist = headlist;
		tiInfo->auth_success = 0;
		
		
		if (tiInfo->clientsock < 0) {	    
		    if(LOGGING==2){
			time(&rawtime);
			timeinfo=localtime(&rawtime);
			sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			sprintf(tmpstring,"Error accepting a connection.\n");
			logger(fileptr,namegen);//Timestamp
			logger(fileptr,tmpstring);
		    }
		    else if(LOGGING==1){
			time(&rawtime);
			timeinfo=localtime(&rawtime);
			sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			printf("%s",namegen);//Timestamp
		    }
		    
		    printf("Error accepting a connection.\n");
		    exit(EXIT_FAILURE);
		}
		else
		    {
			if(LOGGING==2){
			    sprintf(tmpstring,"Got a connection from %s:%d\n",inet_ntoa(tiInfo->clientaddr.sin_addr), tiInfo->clientaddr.sin_port);
			    time(&rawtime);
			    timeinfo=localtime(&rawtime);
			    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			    logger(fileptr,namegen);//Timestamp
			    logger(fileptr,tmpstring);
			}
			else if(LOGGING==1){
			    time(&rawtime);
			    timeinfo=localtime(&rawtime);
			    sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			    printf("%s",namegen);//Timestamp
			    printf("Got a connection from %s:%d\n",inet_ntoa(tiInfo->clientaddr.sin_addr), tiInfo->clientaddr.sin_port);
			}		
			
			pthread_create( &tiInfo->theThread, NULL, threadCallFunction, tiInfo ); 
		    }
	    }	
	    
	    
	    /* At the end, wait until all connections close */ 
	    for (i=topRT; i!=botRT; i = (i+1)%MAX_CONNECTIONS)
		pthread_join(runtimeThreads[i]->theThread, 0 ); 
	    
	    /* Deallocate all the resources */ 
	    for (i=0; i!=MAX_CONNECTIONS; i++)
		free( runtimeThreads[i] );  	
	    
	    close(listensock);
	    return EXIT_SUCCESS;      
	}
    
    return EXIT_SUCCESS; 
}

