/**
 * @file
 * @brief This file implements various utility functions that are
 * can be used by the storage server and client library. 
 */

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"

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


ThreadInfo getThreadInfo(void) { 
  ThreadInfo currThreadInfo = NULL;

  /* Wait as long as there are no more threads in the buffer */ 
  pthread_mutex_lock( &conditionMutex ); 
  while (((botRT+1)%MAX_CONNECTIONS)==topRT)
    pthread_cond_wait(&conditionCond,&conditionMutex); 
  
  /* At this point, there is at least one thread available for us. We
     take it, and update the circular buffer  */ 
  currThreadInfo = runtimeThreads[botRT]; 
  botRT = (botRT + 1)%MAX_CONNECTIONS;

  /* Release the mutex, so other clients can add threads back */ 
  pthread_mutex_unlock( &conditionMutex ); 
  
  return currThreadInfo;
}

/* Function called when thread is about to finish -- unless it is
   called, the ThreadInfo assigned to it is lost */ 
void releaseThread(ThreadInfo me) {
  pthread_mutex_lock( &conditionMutex ); 
  assert( botRT!=topRT ); 
 
  runtimeThreads[topRT] = me; 
  topRT = (topRT + 1)%MAX_CONNECTIONS; 

  /* tell getThreadInfo a new thread is available */ 
  pthread_cond_signal( &conditionCond ); 

  /* Release the mutex, so other clients can get new threads */ 
  pthread_mutex_unlock( &conditionMutex ); 
}


int sendall(const int sock, const char *buf, const size_t len)
{
    size_t tosend = len;
    while (tosend > 0) {
	ssize_t bytes = send(sock, buf, tosend, 0);
	if (bytes <= 0) 
	    break; // send() was not successful, so stop.
	tosend -= (size_t) bytes;
	buf += bytes;
    };
    
    return tosend == 0 ? 0 : -1;
}

/**
 * In order to avoid reading more than a line from the stream,
 * this function only reads one byte at a time.  This is very
 * inefficient, and you are free to optimize it or implement your
 * own function.
 */
int recvline(const int sock, char *buf, const size_t buflen)
{
    int status = 0; // Return status.
    size_t bufleft = buflen;
    
    while (bufleft > 1) {
	// Read one byte from scoket.
	ssize_t bytes = recv(sock, buf, 1, 0);
	if (bytes <= 0) {
	    // recv() was not successful, so stop.
	    status = -1;
	    break;
	} else if (*buf == '\n') {
	    // Found end of line, so stop.
	    *buf = 0; // Replace end of line with a null terminator.
	    status = 0;
	    break;
	} else {
	    // Keep going.
	    bufleft -= 1;
	    buf += 1;
	}
    }
    *buf = 0; // add null terminator in case it's not already there.
    
    return status;
}


void logger(FILE *file, char *message)
{
    fprintf(file,"%s",message);
    fflush(file);
}

char *generate_encrypted_password(const char *passwd, const char *salt)
{
    if(salt != NULL)
	return crypt(passwd, salt);
    else
	return crypt(passwd, DEFAULT_CRYPT_SALT);
}

bool parser(int input, char type)//For parsing parameters in the config file and else
{
    if(type == Table){//No space allowed
	    if(input >= 65 && input <= 90){
		    return true;
		}
	    else if(input >= 97 && input <= 122){
		    return true;
		}
	    else if(input >= 48 && input <= 57){
		    return true;
		}
	    else if(input == 10 || input == 13){
		    return true;
		}
	    else return false;
	}
    else if(type == Key){//No space allowed
	    if(input >= 65 && input <= 90){
		    return true;
		}
	    else if(input >= 97 && input <= 122){
		    return true;
		}
	    else if(input >= 48 && input <= 57){
		    return true;
		}
	    else if(input == 10 || input == 13){
		    return true;
		}
	    else return false;
	}
    else if(type == Value){//Space allowed
	if(input >= 65 && input <= 90){
	    return true;
	}
	else if(input >= 97 && input <= 122){
	    return true;
	}
	else if(input >= 48 && input <= 57){
	    return true;
	}
	else if(input == 32){
	    return true;
	}
	else if(input == 10 || input == 13){
	    return true;
	}
	else return false;
    }
    else return false;
}

struct city* create_city(char* new_name, char *codedvalue)
{
    struct city* new_city = malloc(sizeof(struct city));
    //printf("codedvalue: %s\n", codedvalue);
    new_city->counter = 1;
    strncpy(new_city->name, new_name, sizeof(new_city->name));
    new_city->numocolumns = decode_value(new_city, codedvalue);
    //printf("newcitynumocolumns: %d\n", new_city->numocolumns);
    new_city->next = NULL;
    return new_city;
}

void insert_city(struct city *head, char *new_key, char *value_encoded)
{
    //printf("value_encoded: %s\n", value_encoded);
    struct city* new_city = create_city(new_key, value_encoded);
    //printf("new_city columns: %d\n", new_city->numocolumns);
    if (head != NULL){
	while (head->next != NULL){
	    head = head->next;
	}
	
	head->next = new_city;
    }
    else {
		head = new_city;
    }
}

void modify_city(struct city *tempnode, char *value_encoded)
{
    (tempnode->counter)++;
    tempnode->numocolumns = decode_value(tempnode, value_encoded);
}

int delete_city(struct city **head, char* name)
{
    struct city* before;
    struct city* this;
    
    if ((*head) == NULL){
	//printf("The list is empty\n");
    }
    else{
	if (!strcmp((*head)->name, name)){
			struct city* temp;
			temp = (*head);
	    
	    if ((*head)->next != NULL){
			(*head) = (*head)->next;
			free(temp);
	    }
	    else{
			free((*head));
			(*head) = NULL;
	    }
	    
	    return 0;
	}
	
	before = this = (*head);
	
	while (before->next != NULL){
	    this = before->next;
	    
	    if (!strcmp(this->name, name)){
			before->next = this->next;
			free(this);
		
		return 0;
	    }
	    else{
			before = before->next;	
	    }
	}
	
	printf("The city is not found\n");
    }
    
    return 0;
}

struct city* find_city(struct city* head, char* name)
{
    if (head == NULL)
    {
		printf("head is null");
		return NULL;
    }
    else{
		while (head != NULL)
		{
			if (!strcmp(head->name, name))
			{
				printf("found head: %d\n", head->numocolumns);
				printf("headname: %s\n", head->name);
				return head;
			}
			else
			{
				//printf("traversing through list\n");
				printf("headname: %s\n", head->name);
				head = head->next;
			}
	}
		return NULL;
    }
}

void print_city(struct city* this_city)
{
    if (this_city != NULL){
	int i = 0; //loop counter
	//strncpy(pop, this_city->population, sizeof(pop));
	
	//printf("city name: %s\n", this_city->name);
	//printf("numocolumns: %d\n", this_city->numocolumns);
	//print_column(&this_city->columnlist[0]);
	while(i < this_city->numocolumns){
	    print_column(&this_city->columnlist[i]);
	    i++;
	}
    }
    else {
	printf("the city is NULL\n");
    }
}

void print_list(struct city* head)
{
    if (head != NULL){
	while (head != NULL){
	    print_city(head);
	    head = head->next;
	}
    }
    else {
	printf("The list is empty\n");
    }
    
}

//Get function without overflowing target array
void sget(char *s, int arraylength)
{
    int i=0;
    char c;
    while(i < arraylength-1 && (c=getchar())!='\n')
	{   
	    if(c!=127)
		{
		    s[i]=c;
		    i++;
		}
	}
    s[i]='\0';
} 


int find_index(char tablelist[MAX_TABLES][MAX_TABLE_LEN], char* name)
{
    int i = 0;
    while (i < MAX_TABLES){
	printf("tablelist %d: %s\n", i, tablelist[i]);
	if (strcmp(tablelist[i], name) == 0){
	    return i;
	}
	else i++;
    }
    return -1;
}

int columncopy(struct column *source, struct column *dest)
{
    strncpy(dest->typename, source->typename, sizeof(dest->typename));
    dest->flag = source->flag;
    if(dest->flag == true){
	strncpy(dest->strval, source->strval, sizeof(dest->strval));
    }
    else{
	dest->intval = source->intval;
    }
    return 1;
}  

void cleanstring(char *string)
{
    int i = 0;
    if(string != NULL){
	while(string[i] != '\0'){
	    i++;
	}
	for(; i >= 0; i--){
	    string[i] = '\0';
	}
    }
}

int count_column(char *source)
{
    //reads value string and populates columns in that specific struct
    printf("source: %s\n", source);
    char tempint[1024];
    bool typeflag = false;
    bool strflag = false;
    bool intflag = false;
    int i = 0;//source string address
    int j = 0;//dest column list
    int k = 0;//char copy counter
    k=0;

    while(source[i] != '?' && source[i] != '\0'){
	//printf("source[i] = %c \n", source[i]);
	if(source[i] != '@' && typeflag == true){//write to typename
	    k++;//increment char counter
	}
	else if(source[i] != '$' && strflag == true){//write to strval
	    k++;//increment char counter
	}
	else if(source[i] != '#' && intflag == true){//write to intval
	    tempint[k] = source[i];//copy char to tempint for conversion later
	    k++;//increment char counter
	}
	//get dest ready
	if(source[i] == '@'){//name encountered
	    if(typeflag == true){//already enabled
		typeflag = false;//disable (second '@' encountered)
		k = 0;//reset char counter
	    }
	    else typeflag = true;
	}
	else if(source[i] == '$'){//string encountered
	    if(strflag == true){
		strflag = false;//second '$' encountered
		k = 0;//reset char counter
	    }
	    else strflag = true;
	}
	else if(source[i] == '#'){//int encountered
	    if(intflag == true){
		intflag = false;//second '#' encountered
		tempint[k] = '\0';
		k = 0;//reset char counter
	    }
	    else intflag = true;
	}
	else if(source[i] == '!'){//column ends, go to next column
	    j++;//increment column counter
	}
	i++;//increment i and repeat
    }
    printf("j=%d\n", j);
    return j;
}



int decode_value(struct city *target, char *source)
{
    //reads value string and populates columns in that specific struct
    struct column *dest = target->columnlist;
    printf("source: %s\n", source);
    char tempint[1024];
    bool typeflag = false;
    bool strflag = false;
    bool intflag = false;
    int i = 0;//source string address
    int j = 0;//dest column list
    int k = 0;//char copy counter
    k=0;
    while(source[i] != '?' && source[i] != '\0'){
	//printf("source[i] = %c \n", source[i]);
	if(source[i] != '@' && typeflag == true){//write to typename
	    dest[j].typename[k] = source[i];//copy char
	    k++;//increment char counter
	}
	else if(source[i] != '$' && strflag == true){//write to strval
	    dest[j].flag = true;//set flag to string mode
	    dest[j].strval[k] = source[i];
	    k++;//increment char counter
	}
	else if(source[i] != '#' && intflag == true){//write to intval
	    dest[j].flag = false;//set flag to int mode
	    tempint[k] = source[i];//copy char to tempint for conversion later
	    k++;//increment char counter
	}
	//get dest ready
	if(source[i] == '@'){//name encountered
	    if(typeflag == true){//already enabled
		typeflag = false;//disable (second '@' encountered)
		dest[j].typename[k] = '\0';
		k = 0;//reset char counter
	    }
	    else typeflag = true;
	}
	else if(source[i] == '$'){//string encountered
	    if(strflag == true){
		strflag = false;//second '$' encountered
		dest[j].strval[k] = '\0';
		k = 0;//reset char counter
	    }
	    else strflag = true;
	}
	else if(source[i] == '#'){//int encountered
	    if(intflag == true){
		intflag = false;//second '#' encountered
		tempint[k] = '\0';
		dest[j].intval = atoi(tempint);//convert string to int
		k = 0;//reset char counter
	    }
	    else intflag = true;
	}
	else if(source[i] == '!'){//column ends, go to next column
	    j++;//increment column counter
	}
	i++;//increment i and repeat
    }
    printf("j=%d\n", j);
    return j;
}

void print_column(struct column *column)
{
    //prints a column
    //printf("typename: %s; ", column->typename);
    //printf("value: ");
    if(column->flag==true){
	printf("%s ", column->strval);
    }
    else{
	printf("%d ", column->intval);
    }
    printf("\n");
}

int encode_value(struct column column[10], char *target, int limit)
{
    //encodes <limit> number of columns into one string using protocol
    char tempstring[1024];
    int j=0;
    while(j<limit){
	strcat(target, "@");
	strcat(target, column[j].typename);
	strcat(target, "@");
	if(column[j].flag == true){
	    //char
	    strcat(target, "$");
	    strcat(target, column[j].strval);
	    strcat(target, "$");
	}
	else {
	    //int
	    strcat(target, "#");
	    sprintf(tempstring, "%d", column[j].intval);
	    strcat(target, tempstring);
	    strcat(target, "#");
	}
	if(j<limit){
	    strcat(target, "!");
	}
	j++;//increment column counter
    }
    strcat(target, "?"); 
    return 0;
}

int decode_line(char *received, char *command, char *tablename, char *keyname, char *value, int *counter)
{
    //decodes a line received from client into respective arguments
    bool commandflag = false;
    bool nameflag = false;
    bool keyflag = false;
    bool valueflag = false;
    bool atflag = false;
    bool counterflag = false;
    bool nocounter = true;
    int j=0;//dest counter
    int i=0;//received line counter
    char tempcounter[100];
    while(received[i]!='\0'){
	//puts("in decode_line while loop");
	if(commandflag == true && received[i] != '&'){
	    command[j] = received[i];
	    j++;
	}
	else if(nameflag == true && received[i] != '^'){
	    tablename[j] = received[i];
	    j++;
	}
	else if(keyflag == true && received[i] != '*'){
	    keyname[j] = received[i];
	    j++;
	}
	else if(counterflag == true && received[i] != '~'){
	    tempcounter[j] = received[i];
			j++;
	}
	else if(valueflag == true){
	    value[j] = received[i];
	    j++;
	}
	if(received[i] == '&'){
	    if(commandflag == true){
		commandflag = false;
		command[j] = '\0';
		j=0;
	    }
	    else commandflag = true;
	}
	else if(received[i] == '^'){
	    if(nameflag == true){
		nameflag = false;
		tablename[j] = '\0';
		j=0;
	    }
	    else nameflag = true;
	}
	else if(received[i] == '*'){
			if(keyflag == true){
			    keyflag = false;
			keyname[j] = '\0';
			j=0;
			}
			else keyflag = true;
	}
	else if(received[i] == '@'){
	    valueflag = true;
	    if(atflag == false){
		atflag = true;
		value[j] = '@';
		j++;
	    }
	    //value[j] = received[i];
	    //j++;
	}
	else if(received[i] == '~'){
	    if(counterflag == true){
				counterflag = false;
				tempcounter[j] = '\0';
				j=0;
	    }
	    else {
		valueflag = false;
		value[j-1] = '\0';
		j = 0;	
		counterflag = true;
		nocounter = false;
	    }
	}
	i++;
    }
    if(nocounter == true){
	i = 0;
	while(value[i] != '?'){
	    i++;
	}
	value[i+1] = '\0';
    }
    *counter = atoi(tempcounter);
    //value[j] = '\0';
    return 0;
}

int encode_line(char *type, char *status, char *statustwo, char *retline)
{
    /*encodes a line to be sent back to the client containing
      appropriate arguments*/
    sprintf(retline, "%s %s %s", type, status, statustwo);
    return 0;
}

int encode_retval(struct column column[10], char *retval, int limit)
{
    //encodes values from the columns into a line for further usage
    char tempstring[1024];
    int j=0;
    while(j<limit){
	strcat(retval, column[j].typename);
	strcat(retval, " ");
	if(column[j].flag == true){
	    //char
	    strcat(retval, column[j].strval);
	}
	else {
	    //int
	    sprintf(tempstring, "%d", column[j].intval);
	    strcat(retval, tempstring);
	}
	if(j<limit-1){
	    strcat(retval, ",");
	}
	j++;//increment column counter
    }
    strcat(retval, " END");
    return 0;
}

int query_argument(struct queryarg *querylist, char *values)
{
    bool firstflag = false;
    bool secondflag = false;
    bool intflag = false;
    bool opflag = false;
    char tempint[1024];
    int i = 0; //values counter
    int j = 0; //array y index
    int k = 0; //array x index
    while(values[i] != '\0') {
	if(firstflag == true && values[i] != '@') {
	    querylist->firstarg[j][k] = values[i];
	    k++;
	}
	else if(secondflag == true && values[i] != '$'){
	    querylist->secondarg[j][k] = values[i];
	    k++;
	}
	else if(opflag == true && values[i] != '&'){
	    querylist->operator[j] = values[i];
	}
	else if(intflag == true && values[i] != '#'){
	    tempint[k] = values[i];
	    k++;
	}
	if(values[i] == '@'){
	    if(firstflag == true){
		firstflag = false;
		querylist->firstarg[j][k] = '\0';
		k = 0;
	    }
	    else firstflag = true;
	}
	else if(values[i] == '#'){
	    if(intflag == true){
		intflag = false;
		tempint[k] = '\0';
		k = 0;
		querylist->max_keys = atoi(tempint);
	    }
	    else intflag = true;
	}
	else if(values[i] == '&'){
	    if(opflag == true){
		opflag = false;
	    }
	    else opflag = true;
	}
	else if(values[i] == '!'){
	    j++;
	}
	else if(values[i] == '$'){
	    if(secondflag == true){
		secondflag = false;
		querylist->secondarg[j][k] = '\0';
		k = 0;
	    }
	    else secondflag = true;
	}
	i++;
    }
    return j+1;//number of query arguments
}

int query_compare(struct queryarg *querylist, struct city *target, int *querynum)
{
    //what to do?
    //travel through querylist and find matching name (firstarg) first
    //then fetch value and flag from that column
    //do the operation according to query operator
    //finally return result (true = 1, false = 0)
    //Possible Errors:
    //1. mismatched type (parse by individual characters)
    //2. duplicate (handled using column-by-column comparison)
    //3. wrong operator (if type is not int and '<' or '>' is used, return error.)
    int i = 0;//columnlist counter
    int j = 0;//querynum counter
    int tempint;
    bool matchflag = true;//to be set false if mismatch occurs
    printf("target name: %s\n", target->name);
    while(i < target->numocolumns) {
	
	//puts("i<target->numocolumns");
	//traverse until the end of the columnlist
	if(j > *querynum){
	    //puts("increment i, reset j");
	    printf("target name: %s\n", target->columnlist[i].typename);
	    //end of querylist reached
	    i++;//increment columnlist
	    j = 0;//reset querynum counter
	}
	//printf("pre-comparison, target typename: %s\n", target->columnlist[i].typename);
	if(strcmp(querylist->firstarg[j], target->columnlist[i].typename) == 0){
	    //matching typename
	    if(target->columnlist[i].flag == true){
		//string comparison
		puts("string comparison");
		if(querylist->operator[j] != '='){
		    //invalid operator ('=' only for strings)
		    puts("invalid operand");
		    return -1;
		}
		else {
		    if(strcmp(querylist->secondarg[j], target->columnlist[i].strval) == 0){
			//comparison successful
			//do nothing
			puts("string comparison successful");
		    }
		    else {
			//comparison failed
			puts("string comparison failed");
			matchflag = false;
		    }
		}
	    }
	    else if(target->columnlist[i].flag == false) {
		//int comparison
		tempint = atoi(querylist->secondarg[j]);
		printf("int comparison, tempint = %d\n", tempint);
		printf("target intval = %d\n", target->columnlist[i].intval);
		if(querylist->operator[j] == '<'){
		    //less than secondarg
		    if(target->columnlist[i].intval >= tempint) {
			matchflag = false;
			puts("int comparison < failed");
		    }
		    else {
			//do nothing
			puts("int comparison < successful");
		    }
		}
		else if(querylist->operator[j] == '>'){
		    //greater than secondarg
		    if(target->columnlist[i].intval <= tempint) {
			matchflag = false;
			puts("int comparison > failed");
		    }
		    else {
			//do nothing
			puts("int comparison > successful");
		    }
		}
		else if(querylist->operator[j] == '='){
		    //equal to secondarg
		    if(target->columnlist[i].intval != tempint) {
			matchflag = false;
			puts("int comparison = failed");
		    }
		    else {
			//do nothing
			puts("int comparison = successful");
		    }
		}
	    }
	    else matchflag = false;
	}
	else {
	    // no match, move on
	    //j++;//increment querylist index
	    //printf("very weird error indeed.\n");
	    //rinse and repeat
	}
	j++;
    }
    if(matchflag == true){
	printf("matchflag correct\n");
	return 1;//all query
    } 
    else return 0;
}

int query_write(char keylist[1000][1024], struct queryarg *querylist, struct city *head, int *limit, int *querynum)
{
    int status= 0;
    int i = 0;//keylist counter
    printf("limit is %d\n", *limit);
    //*limit++;
    //takes input arguments and compare columns in the table according to it
    while(head != NULL) {
	printf("head name: %s\n", head->name);
	status = query_compare(querylist, head, querynum);
	printf("status = %d\n", status);
	if(status == -1){
	    //error occurred
	    printf("error occurred\n");
	    return -1;
	}
	else if(status == 1 && i < *limit) {
	    printf("i is less than limit\n");
	    printf("head->name: %s\n", head->name);
	    printf("keylist[%d]: %s\n", i, keylist[i]);
	    if(head->name[0] != '\0' || head->name[0] != ' '){
	       	strncpy(keylist[i], head->name, sizeof(keylist[i]));
		printf("written to keylist\n");
		i++;
	    }
	}
	else {
	    // do nothing
	    printf("do nothing\n");
	}
	head = head->next;
	printf("move on to next head\n");
    }
    return 0;
}

int check_column(struct config_params *param)
{
    int i = 0, j = 0, status = 0;
    for (i = 0; i < param->num_tables; i++){
		int num_column = param->num_columns[i];
		//printf("table name: %s\n", param->tablelist[i]);
		//printf("num_column: %d\n", num_column);
	
		for (j = 0; j < num_column; j++){
			struct column *curr = &(param->columnlist[i][j]);
			bool flag = curr->flag;
			
			int k = 0, h = 0;
			
			for (k = 0; k < param->num_tables; k++){
				// Compare only with different tables
				if (k != i){
					int match_count = 0;
					int num_column_temp = param->num_columns[k];
				
					for (h = 0; h < num_column_temp; h++){
						struct column *temp = &(param->columnlist[k][h]);
						bool flag_temp = temp->flag;
			
						int k = 0, h = 0;
		
						for (k = 0; k < param->num_tables; k++)
						{
							// Compare only with different tables
							if (k != i)
							{
								if (strcmp(param->tablelist[i], param->tablelist[k]) == 0)
								{
									int match_count = 0;
									int num_column_temp = param->num_columns[k];
			
									for (h = 0; h < num_column_temp; h++)
									{
										struct column *temp = &(param->columnlist[k][h]);
										bool flag_temp = temp->flag;
				
										// starts comparing
										if (flag == flag_temp) // Same type
										{
											if (strcmp(curr->typename, temp->typename) == 0)
											{
												//printf("curr->typename: %s", curr->typename);
												//printf("temp->typename: %s", temp->typename);
												match_count++;
											}
										}
				
										if (match_count == num_column)
										{
											status = -1;
											return status;
										}
									}					
								}
							}
						}	
					}
				}	
		 	}
		}
	}
	
	return status;
}

int queryparse(char *in, char *out)
{
    printf("in queryparse\n");
    printf("in: %s\n", in);
    char *pch;
    printf("pch created\n");
    int stat = 0;//no error
    printf("before strtok\n");
    pch = strtok(in, ",");
    printf("after strtok\n");
    if(pch != NULL){
	printf("pch: %s\n", pch);
    }
    else printf("pch is NULL\n");
    stat = argparse(pch, out);
    if(stat != 0){
	return stat;//invalid params
    }
    while(pch != NULL){
	pch = strtok(NULL, ",");
	printf("pch: %s\n", pch);
	if(pch != NULL){
	   stat = argparse(pch, out);
	   if(stat != 0){
	       return stat;//invalid params
	   }
	}
    }
    return stat;//valid params
}

int argparse(char *in, char *out)
{
    char firstarg[1024];
    char secondarg[1024];
    char operator_sign;
    int i = 0;//in counter
    int j = 0;//out counter
    bool beginflag = false;
    bool firstflag = false;
    bool secondflag = false;
    bool opflag = false;
    bool intflag = false;
    bool floatflag = false;
    char tempout[1024];

    while(in[i] != '\0'){
	if(firstflag == true && in[i] != ' ' && in[i] != '=' && in[i] != '<' && in[i] != '>'){
	    firstarg[j] = in[i];
	    j++;
	}
	if(secondflag == true && beginflag == false){
	    //secondarg[j] = in[i];
	    //j++;
	    if(in[i] != ' '){
		//secondarg[j] = in[i];
		//j++;
		beginflag = true;
		if(in[i] == '-' || (in[i] <= '9' && in[i] >= '0')){
		    intflag = true;
		}
		else intflag = false;
	    }
	}
	if(secondflag == true && beginflag == true){
	    if(in[i] == '.'){
		floatflag = true;
	    }
	    secondarg[j] = in[i];
	    j++;
	}
	if(firstflag == true && (in[i] == ' ' || in[i] == '=' || in[i] == '<' || in[i] == '>')){
	    firstflag = false;
	    opflag = true;
	    //beginflag = false;
	    firstarg[j] = '\0';
	    j = 0;
 	}
	if(beginflag == false && in[i] != ' ' && secondflag == false){
	    beginflag = true;
	    firstflag = true;
	    firstarg[j] = in[i];
	    j++;
	}
	if(opflag == true){
	    if(in[i] == '<' || in[i] == '>' || in[i] == '='){
		operator_sign = in[i];
		opflag = false;
		beginflag = false;
		secondflag = true;
	    }
	}
	i++;
    }
    secondarg[j] = '\0';

    printf("firstarg is: %s\n", firstarg);
    printf("operator is: %c\n", operator_sign);
    printf("secondarg is: %s\n", secondarg);
    if(intflag == true){
	sprintf(tempout, "@%s@&%c&$%s$!", firstarg, operator_sign, secondarg);
    }
    else if(intflag == false){
	if(operator_sign == '=' && floatflag == false){
	sprintf(tempout, "@%s@&%c&$%s$!", firstarg, operator_sign, secondarg);
	}
	else return -1;//return invalid params
    }
    printf("tempout: %s\n", tempout);
    strcat(out, tempout);
    return 0;
}

int decode_queryret(char *ret_buffer, char **keylist)
{
    //#<number_of_matching_keys>#
    //@<key_name>@
    //<prev>!<next>
    bool intflag = false;
    bool stringflag = false;
    int i = 0;//buffer counter
    int j = 0;//keylist column counter
    int k = 0;//keylist row counter
    char intstring[1024];
    int num_match = 0;
    while(ret_buffer[i] != '\0'){
	if(intflag == true && ret_buffer[i] != '#'){
	    intstring[k] = ret_buffer[i];
	    k++;
	}
	if(stringflag == true && ret_buffer[i] != '@' && ret_buffer[i] != '!'){
	    keylist[j][k] = ret_buffer[i];
	    k++;
	}
	
	if(ret_buffer[i] == '#'){
	    if(intflag == false){
		intflag = true;
	    }
	    else {
		//if true
		intflag = false;
		intstring[k] = '\0';
		k = 0;
	    }
	}
	else if(ret_buffer[i] == '@'){
	    if(stringflag == false){
		stringflag = true;
	    }
	    else {
		keylist[j][k] = '\0';
		k = 0;
	    }
	}
	else if(ret_buffer[i] == '!'){
	    printf("keylist[%d]: %s\n", j, keylist[j]);
	    j++;
	    //stringflag = false;
	}
	i++;
    }
    num_match = atoi(intstring);
    return num_match-1;
}

void encode_queryret(int num_match, char keylist[1000][1024], char *retstring)
{
    //same encoding as above
    int i = 1;
    cleanstring(retstring);
    sprintf(retstring, "&QUERY&$SUCCESS$#%d#", num_match);
    while(keylist[i][0] != '\0'){
	strcat(retstring, "@");
	strcat(retstring, keylist[i]);
	strcat(retstring, "@");
	strcat(retstring, "!");
	i++;
    }
}

void add_equal(char *in)
{
    //reads client input and deletes spaces
    int i = 1;//in counter
    bool stringflag = false;
    while(in[i] != '\0'){
	if(in[i-1] != '<' && in[i-1] != '>' && in[i-1] != '=' && in[i-1] != ',' && in[i-1] != ' '){
	    if(in[i] == ' ' && stringflag == false){
		if(in[i+1] != '<' && in[i+1] != '>' && in[i+1] != '=' && in[i+1] != ',' && in[i+1] != ' '){
		    in[i] = '=';
		    stringflag = true;
		}
	    }
	}
	if(in[i] == ','){
	    stringflag = false;
	}
	i++;
    }
}
