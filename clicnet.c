/**
 * @file
 * @brief This file implements a "very" simple sample client.
 * 
 * The client connects to the server, running at SERVERHOST:SERVERPORT
 * and performs a number of storage_* operations. If there are errors,
 * the client exists.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "storage.h"
#include "utils.h"

#define SERVERHOST "localhost"
#define SERVERPORT 1111
#define SERVERUSERNAME "admin"
#define SERVERPASSWORD "dog4sale"
#define TABLE "marks"
#define KEY "ece297"
#define LOGGING 0 //Client-side logging

FILE *fileptr;//Global file pointer variable

// Additional constants
// End of additional constants

// Custom function declarations

// End of custom function declarations

/**
 * @brief Start a client to interact with the storage server.
 *
 * If connect is successful, the client performs a storage_set/get() on
 * TABLE and KEY and outputs the results on stdout. Finally, it exists
 * after disconnecting from the server.
 */
int main(int argc, char *argv[]) {
  int optionnum=0;
  char usernamen[MAX_USERNAME_LEN];
  char password[MAX_ENC_PASSWORD_LEN];
  char tablename[MAX_TABLE_LEN];
  char keyname[MAX_KEY_LEN];
  char predikate[1024];
  int max_keys = 0;
  char **keylist = (char **)malloc(1000*sizeof(char *));
  int i = 0;
  for(i = 0; i < 1000; i++){
      keylist[i] = (char *)malloc(sizeof(char)*1024);
  }
  for(i = 0; i < 1000; i++){
      cleanstring(keylist[i]);
  }
  void *conn;
  struct storage_record r;
  int status=0;

  if(LOGGING==2){//logging
    //File name generation
    time_t rawtime;
    struct tm * timeinfo;
    char namegen[1024];
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    sprintf(namegen,"Client-%.4d-%.2d-%.2d-%.2d-%.2d-%.2d.log",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
    fileptr=fopen(namegen,"w");
  }
  //End of file name generation

  printf("*****ECE297 client interface*****\n");
  printf("1) Connect to server\n");
  printf("2) Authenticate client\n");
  printf("3) Retrieve data\n");
  printf("4) Set data\n");
  printf("5) Query data\n");
  printf("6) Disconnect from server\n");
  printf("7) Exit client interface\n");
  printf("*********************************\n");
  printf("\nPlease select one option:");
  scanf("%d",&optionnum);
  getchar();

  if(optionnum==7)
  {
    printf("Option #7: Exit program selected.\n");
    printf("Exiting program...\n");
  }

  while(optionnum!=7){
      printf("optionnum = %d\n", optionnum);
      if(optionnum==1)//connect
    {
	int portnum;
	char hostnamen[1024];
	
	printf("Option #1: Connect to server selected.\n");
	printf("Enter hostname:");
	sget(hostnamen,MAXLEN+1);//get hostname as a string
	printf("Enter TCP port number:");
	scanf("%d",&portnum);
	printf("Connecting to %s:%d ...\n",hostnamen,portnum);
	printf("hostname: %s\n",hostnamen);
	printf("portnum: %d\n",portnum);
	// Connect to server
	conn = storage_connect(hostnamen,portnum);
	if(!conn) {
	  printf("Cannot connect to server @ %s:%d. Error code: %d.\n", hostnamen, portnum, errno);
	  return -1;
	}
	printf("\n***Connection to %s at port %d successful.***\n",hostnamen,portnum);	

      }
    else if(optionnum==2)//authenticate
    {
		//usernamen und password
		printf("Option #2: Authenticate client selected.\n");
		printf("Enter username:");
		//scan username
		sget(usernamen,MAXLEN+1);
		printf("Enter password:");
		//scan password
		sget(password,MAXLEN+1);
		  
		// Authenticate the client.
		status = storage_auth(usernamen, password, conn);
		if(status != 0) {
			printf("storage_auth failed with username '%s' and password '%s'. Error code: %d.\n", usernamen, password, errno);
			storage_disconnect(conn);
			return status;
		}
    }
    else if(optionnum==3)//gets
      {
	//tablename and keyname
	printf("Option #3: Retrieve server data selected.\n");
	printf("Enter table name:");   
        gets(tablename);
	printf("Enter key:");
        gets(keyname);

	// Issue storage_get
	status = storage_get(tablename,keyname, &r, conn);
	if(status != 0) {
	  printf("storage_get failed. Error code: %d.\n", errno);
	  storage_disconnect(conn);
	  return status;
	}
    }
    else if(optionnum==4)//sets
      {
	char vall[1024];
	
	//tablename and key
	printf("Option #4: Modify server data selected.\n");
	printf("Enter table name:");
	gets(tablename);
	printf("Enter key:");
	gets(keyname);
	strcpy(r.value, "");
	printf("Enter value:");
	sget(vall, 1024);

	// Issue storage_set
	strncpy(r.value, vall, sizeof r.value);
	printf("VALUE: %s\n", r.value);
	status = storage_set(tablename, keyname, &r, conn);
	if(status != 0) {
	  printf("storage_set failed. Error code: %d.\n", errno);
	  storage_disconnect(conn);
	  return status;
	}
      }
    else if(optionnum==5){//query
	/*
	  printf("Option #5: Query data selected\n");
	  printf("Enter table name:");
	  sget(tablename, 1024);
	  printf("Enter predicates:");
	  sget(predikate, 1024);
	  printf("Enter maximum limit for the number of keys:");
	  scanf("%d", &max_keys);
	  status = storage_query(tablename, predikate, keylist, max_keys, conn);
	*/
	//printf("Enter predicates:");
	//sget(predikate, 1024);//col<-1
	//printf("predikate: %s\n", predikate);
	printf("before found_keys\n");
	int foundkeys = storage_query("inttbl", "col<-1", keylist, MAX_RECORDS_PER_TABLE, conn);
	foundkeys = storage_query("inttbl", "col > 10", keylist, MAX_RECORDS_PER_TABLE, conn);
    }
    else if(optionnum==6)//disconnect
      {
	printf("Option #6: Disconnect from server selected.\n");
	status = storage_disconnect(conn);
	if(status != 0) {
	  printf("storage_disconnect failed. Error code: %d.\n", errno);
	  return status;
	}
	printf("Disconnecting from server...\n");
        printf("\n***Disconnected from server***\n");
      }
    else if(optionnum==7)//exit
      {
	printf("Option #7: Exit program selected.\n");
	printf("Exiting program...\n");
	return 0;
      }
    else{
      printf("Non-valid option selected, exiting program...\n");
      return 0;

    }
    printf("\n\n\n");
    printf("*****ECE297 client interface*****\n");
    printf("1) Connect to server\n");
    printf("2) Authenticate client\n");
    printf("3) Retrieve data\n");
    printf("4) Set data\n");
    printf("5) Query data\n");
    printf("6) Disconnect from server\n");
    printf("7) Exit client interface\n");
    printf("*********************************\n");
    printf("\nPlease select one option:");
    scanf("%d",&optionnum);

    if(optionnum<6){
      getchar();
    }

  }
  
  // Exit
  return 0;
}


 
