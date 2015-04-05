/**
 * @file
 * @brief This file contains the implementation of the storage server
 * interface as specified in storage.h.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include "storage.h"
#include "utils.h"

#define LOGGING 0 //Client-side logging


/**
 * @brief This is just a minimal stub implementation.  You should modify it 
 * according to your design.
 */

void* storage_connect(const char *hostname, const int port)
{
	// Create a socket.
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	
	if (sock < 0){
	  printf("if(sock<0)\n");
	  return NULL;
	}
	
	if (hostname == NULL)
	{
		errno = ERR_INVALID_PARAM;
		return NULL;
	}

	time_t rawtime;
	struct tm * timeinfo;
	char namegen[1024];

	// Get info about the server.
	struct addrinfo serveraddr, *res;
	memset(&serveraddr, 0, sizeof serveraddr);
	serveraddr.ai_family = AF_UNSPEC;
	serveraddr.ai_socktype = SOCK_STREAM;
	char portstr[MAX_PORT_LEN];
	snprintf(portstr, sizeof portstr, "%d", port);
	int status = getaddrinfo(hostname, portstr, &serveraddr, &res);
	if (status != 0){
	  return NULL;
	}

	// Connect to the server.
	status = connect(sock, res->ai_addr, res->ai_addrlen);
	
	// check connection
	if (status != 0){
	  errno = ERR_CONNECTION_FAIL;
	  return NULL;
	}
	
	if(LOGGING==2){
	  time ( &rawtime );
	  timeinfo = localtime ( &rawtime );
	  sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	  sprintf(portstr,"%d\n",port);
	  logger(fileptr,namegen);//Timestamp
	  logger(fileptr,portstr);//Records the port number in log file
	}
	else if(LOGGING==1){
	  time ( &rawtime );
	  timeinfo = localtime ( &rawtime );
	  sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	  sprintf(portstr,"%d\n",port);
	  printf("%s",namegen);//Timestamp
	  printf("port %d\n",port);
	}
	return (void*) sock;
}


int storage_auth(const char *username, const char *passwd, void *conn)
{
	if (username == NULL || passwd == NULL || conn == NULL)
	{
		errno = ERR_INVALID_PARAM;
		return -1;
	}

	// Connection is really just a socket file descriptor.
	int sock = (int)conn;
	
	struct tm * timeinfo;
	time_t rawtime;
	char namegen[1024];
	
	// check connection
	int yes = 1;
  	int status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  	if (status != 0)
  	{
  		errno = ERR_CONNECTION_FAIL;
  		return -1;
  	}
  	else
  	{
		// Send some data.
		char buf[MAX_CMD_LEN];
		memset(buf, 0, sizeof buf);
		char *encrypted_passwd = generate_encrypted_password(passwd, NULL);
		snprintf(buf, sizeof(buf), "&AUTH&^%s^*%s*?\n", username, encrypted_passwd);
		if (sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof buf) == 0){
		
			// PARSING AUTH PROTOCOL
			int error = 0;
		  	scan_string(buf);
		  	
		  	
			int status = 0;
			struct config_params params;
			struct storage_record record_temp;
			struct bigstring str;
			int max_keys = 10;
			char keynames[10][100];
			error = yyparse(&params, &record_temp, &str, &max_keys, keynames, &status);		  	
		  	
    		if (error == -1)
    		{
    			return -1;
    		}
		    
	
		  if(LOGGING==2){
			// sprintf(buf,"AUTH %s %s\n");
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			logger(fileptr,namegen);//Timestamp
			logger(fileptr,buf);//Records username and encrypted password in log file
		        logger(fileptr,"\n");
		  }
		  else if(LOGGING==1){
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
			printf("%s",namegen);//Timestamp
			printf("\nAUTH %s %s\n",username,encrypted_passwd);
		  }
		  return 0;
		}

		return -1;
  	}
}


int storage_get(const char *table, const char *key, struct storage_record *record, void *conn)
{
	// check parameter
	
	if (table == NULL || key == NULL || record == NULL || conn == NULL)
	{
		errno = ERR_INVALID_PARAM;
		return -1;
	}	
	
	int n = 0;
	while (table[n] != '\0')
	{
		if (!parser(table[n], 'T'))
		{
			errno = ERR_INVALID_PARAM;
		}
		n++;
	}
	
	n = 0;
	while (key[n] != '\0')
	{
		if (!parser(key[n], 'K'))
		{
			errno = ERR_INVALID_PARAM;
		}
		n++;
	}

	time_t rawtime;
	struct tm * timeinfo;
	char namegen[1024];
	// Connection is really just a socket file descriptor.
	int sock = (int)conn;
	
		
	// check connction
	int yes = 1;
  	int status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  	if (status != 0)
  	{
  		errno = ERR_CONNECTION_FAIL;
  		return -1;
  	}
  	
  	
  	// Check for whitespace in Table and key name
  	int i = 0;
  	for (i = 0; i < MAX_TABLE_LEN; i++)
  	{
  		if (table[i] == '\0')
  		{
  			break;
  		}
  		else
  		{
  			if (table[i] == ' ')
  			{
  				status = -1;
  			}
  		}
  	}
  	
  	if (status == -1)
  	{
  		errno = ERR_INVALID_PARAM;
  		return -1;
  	}
  	
   	for (i = 0; i < MAX_KEY_LEN; i++)
  	{
  		if (key[i] == '\0')
  		{
  			break;
  		}
  		else
  		{
  			if (key[i] == ' ')
  			{
  				status = -1;
  			}
  		}
  	}
  	
  	if (status == -1)
  	{
  		errno = ERR_INVALID_PARAM;
  		return -1;
  	}
  	
  	
	// Send some data.
	char buf[MAX_CMD_LEN];
	memset(buf, 0, sizeof buf);
	snprintf(buf, sizeof buf, "&GET&^%s^*%s*?\n", table, key);

	if (sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof buf) == 0) {
	    //Parsing GET
		struct config_params param;
		struct bigstring str;
		int max_keys = 10;
		char keynames[10][100];
	    int status = 0;
	    
	    
	    printf("buf: %s\n", buf);
	    scan_string(buf);
	    int error = 0;
	    strcpy(record->value, "");
	    error = yyparse(&param, record, &str, &max_keys, keynames, &status);
	    if (error == -1)
		{
		    return -1;
		}
		
		printf("record->value: %s\n", record->value);
		int temp_counter = (int) record->metadata[0];
		printf("counter: %d\n", temp_counter);
		
	    if(LOGGING==2){
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		logger(fileptr,namegen);//Timestamp
		//logger(fileptr,error);//Records the retrieved table and key in log file
		logger(fileptr,"\n");
	    }
	    else if(LOGGING==1){
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		printf("%s",namegen);//Timestamp
		//printf("%s\n",success);
	    }
	    return 0;
	}
	return -1;
}


/**
 * @brief This is just a minimal stub implementation.  You should modify it 
 * according to your design.
 */
int storage_set(const char *table, const char *key, struct storage_record *record, void *conn)
{
	// check parameter
	if (table == NULL || key == NULL || conn == NULL)
	{
		errno = ERR_INVALID_PARAM;
		return -1;
	}	
	
	int n = 0;
	while (table[n] != '\0')
	{
		if (!parser(table[n], 'T'))
		{
			errno = ERR_INVALID_PARAM;
			return -1;
		}
		n++;
	}
	
	n = 0;
	while (key[n] != '\0')
	{
		if (!parser(key[n], 'K'))
		{
			errno = ERR_INVALID_PARAM;
			return -1;
		}
		n++;
	}
	
  	// Check for whitespace in Table and key name
  	int i = 0, status = 0;
  	for (i = 0; i < MAX_TABLE_LEN; i++)
  	{
  		if (table[i] == '\0')
  		{
  			break;
  		}
  		else
  		{
  			if (table[i] == ' ')
  			{
  				status = -1;
  			}
  		}
  	}
  	
  	if (status == -1)
  	{
  		errno = ERR_INVALID_PARAM;
  		return -1;
  	}
  	
   	for (i = 0; i < MAX_KEY_LEN; i++)
  	{
  		if (key[i] == '\0')
  		{
  			break;
  		}
  		else
  		{
  			if (key[i] == ' ')
  			{
  				status = -1;
  			}
  		}
  	}
  	
  	if (status == -1)
  	{
  		errno = ERR_INVALID_PARAM;
  		return -1;
  	}	
	
	

	time_t rawtime;
	struct tm * timeinfo;
	char namegen[1024];
	// Connection is really just a socket file descriptor.
	int sock = (int)conn;
	
	// check connction
	int yes = 1;
  	status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  	if (status != 0)
  	{
  		errno = ERR_CONNECTION_FAIL;
  		return -1;
  	}	

	// Send some data.
	char buf[MAX_CMD_LEN];
	memset(buf, 0, sizeof buf);
	
	struct config_params param;
	struct storage_record record_temp;
	struct bigstring str;
	int max_keys = 10;
	char keynames[10][100];
	
	if (record == NULL)
	{
		snprintf(buf, sizeof buf, "&SET&^%s^*%s*@%s@?\n", table, key, "NULL");
	}
	else if(strcmp(record->value, "NULL") == 0)
	{
		snprintf(buf, sizeof buf, "&SET&^%s^*%s*@%s@?\n", table, key, record->value);
	}
	else
	{
		strcpy(str.string, "");
		//printf("record->value: %s\n\n\n", record->value);
		scan_string(record->value);
		int error = 0;
		//printf("\n\n\nstr.string: %s\n\n\n", str.string);
		error = yyparse(&param, &record_temp, &str, &max_keys, keynames, &status);
		//printf("\n\n\nstr.string: %s\n\n\n", str.string);
		if (error == -1)
		    {
			return -1;
		    }
		//printf("RECORD->VALUE: %s\n", str.string);
		int counter = (int) record->metadata[0];
		
		snprintf(buf, sizeof buf, "&SET&^%s^*%s*%s~%d~!?\n", table, key, str.string, counter);
		printf("buf: %s", buf);
	}
	printf("BUFFER: %s\n", buf);
	if (sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof buf) == 0) {
		// Parsing SET
	    scan_string(buf);
	    int error = 0;
	    error = yyparse(&param, &record, &str, &max_keys, keynames, &status);
	    if (error == -1)
		{
		    return -1;
		}
	    
	    // reseting record metadata to 0
	    memset(&record, 0, sizeof record);
	    
	    if(LOGGING==2){
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		logger(fileptr,namegen);//Timestamp
		logger(fileptr,buf);//Records the value modified from table and key in log file
		logger(fileptr,"\n");
	    }
	    else if(LOGGING==1){
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		sprintf(namegen,"%.4d-%.2d-%.2d-%.2d-%.2d-%.2d: ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
		printf("%s",namegen);//Timestamp
		printf("SET %s %s %s\n", table, key, record->value);
	    }
	    return 0;
	}
	
	return -1;
}

/**
 * @brief This is just a minimal stub implementation.  You should modify it 
 * according to your design.
 */
int storage_disconnect(void *conn)
{
	if (conn == NULL)
	{
		errno = ERR_INVALID_PARAM;
		return -1;
	}
	
	// Cleanup
	int sock = (int)conn;
	int yes = 1;
  	int status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  	if (status != 0)
  	{
  		errno = ERR_CONNECTION_FAIL;
  		return -1;
  	}	

	close(sock);
	return 0;
}

int storage_query(const char *table, const char *predicates, char **keys, const int max_keys, void *conn)
{
    //finds matching keys using predicates given and return the number of matching keys
    //first, parse the predicates into separate conditions
    printf("table: %s\n", table);
    printf("predicate: %s\n", predicates);
    printf("max_keys: %d\n", max_keys);
    int matching_keys=0;
    int questatus = 0;
    int lim = 0;
    int stat = 0;
    int i = 0;
    int j = 0;
    int sock = (int)conn;
    char buf[MAX_CMD_LEN];
    char command[20];
    char status[50];
    char reason[50];
    char predicate_copy[1024];
    
    add_equal(predicates);
    while(predicates[i] != '\0'){
	predicate_copy[i] = predicates[i];
	i++;
    }
    predicate_copy[i] = '\0';
    i = 0;
    printf("predicate_copy: %s\n", predicate_copy);
    printf("add equal successful\n");
    //printf("predicates: %s\n", predicates);
    sprintf(buf, "&QUERY&^%s^#%d#", table, max_keys);
    printf("sprintf successful\n");
    stat = queryparse(predicate_copy, buf);
    printf("queryparse successful\n");
    //printf("output: %s\n", buf);
    strcat(buf, "\n");
    //printf("buf: %s\n", buf);
    if(sendall(sock, buf, strlen(buf)) == 0 && recvline(sock, buf, sizeof(buf)) == 0){
	//printf("received buffer: %s\n", buf);
	printf("recvline successful, query_buf: %s\n", buf);
	i++;
	while(buf[i] != '&'){
	    command[j] = buf[i];
	    i++;
	    j++;
	}
	command[j] = '\0';
	printf("command: %s\n", command);
	i++;
	i++;
	j = 0;
	while(buf[i] != '$'){
	    status[j] = buf[i];
	    i++;
	    j++;
	}
	status[j] = '\0';
	printf("status: %s\n", status);
	i++;
	j = 0;
	if(buf[i] == '^'){
	    i++;
	    while(buf[i] != '^'){
		reason[j] = buf[i];
		i++;
		j++;
	    }
	    reason[j] = '\0';
	    printf("reason: %s\n", reason);
	}
	else {
	    matching_keys = decode_queryret(buf, keys);
	}
	j = 0;
	printf("matching keys = %d\n", matching_keys);
	if(strcmp(status, "FAIL") == 0){
	    if(strcmp(reason, "TABLE")){
		printf("Query failed, table does not exist.\n");
	    }
	    return -1;
	}
	printf("max_keys = %d\n", max_keys);
    }
    //return conditions
    if(matching_keys > max_keys){
	//# of matching keys exceeds the limit
	return matching_keys;
    }
    else if(max_keys == 0){
	//given a limit of 0
	
	return 0;
    }
    else return matching_keys;//all normal, return # of matching keys
}
