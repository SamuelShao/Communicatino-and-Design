/**
 * @file
 * @brief This file declares various utility functions that are
 * can be used by the storage server and client library.
 */

#ifndef	UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <pthread.h>
#include "storage.h"

/*Custom definitions*/
#define Table 'T'
#define Key 'K'
#define Value 'V'
#define MAXLEN 1023
/*End of custom definitions*/

//////////////////////////// M4 /////////////////////////////////

struct _ThreadInfo { 
	struct sockaddr_in clientaddr;
	socklen_t clientaddrlen; 
	int clientsock; 
	pthread_t theThread;
	
	FILE *fileptr;
	struct config_params* params;
	struct city **headlist;
	int auth_success;	 
}; 
typedef struct _ThreadInfo *ThreadInfo; 

/*  Thread buffer, and circular buffer fields */ 
extern ThreadInfo runtimeThreads[MAX_CONNECTIONS]; 
extern unsigned int botRT, topRT;

/* Mutex to guard SET statements */
extern pthread_mutex_t setMutex;

/* Mutex to guard print statements */ 
extern pthread_mutex_t  printMutex; 

/* Mutex to guard condition -- used in getting/returning from thread pool*/ 
extern pthread_mutex_t conditionMutex;
/* Condition variable -- used to wait for avaiable threads, and signal when available */ 
extern pthread_cond_t  conditionCond;

ThreadInfo getThreadInfo(void);
void releaseThread(ThreadInfo me);
//////////////////////////// M4 /////////////////////////////////

int count_column(char *source);

struct bigstring{
	char string[1024];
};

/**
 * @brief Any lines in the config file that start with this character 
 * are treated as comments.
 */
static const char CONFIG_COMMENT_CHAR = '#';

/**
 * @brief The max length in bytes of a command from the client to the server.
 */
#define MAX_CMD_LEN (1024 * 8)

/**
 * @brief A macro to log some information.
 *
 * Use it like this:  LOG(("Hello %s", "world\n"))
 *
 * Don't forget the double parentheses, or you'll get weird errors!
 */
#define LOG(x)  {printf x; fflush(stdout);}

/**
 * @brief A macro to output debug information.
 * 
 * It is only enabled in debug builds.
 */
#ifdef NDEBUG
#define DBG(x)  {}
#else
#define DBG(x)  {printf x; fflush(stdout);}
#endif

struct column{
    char typename[MAX_STRTYPE_SIZE];
    bool flag; /* char[SIZE]==true, int==false */
    union
    {
		int intval;
		char strval[MAX_STRTYPE_SIZE];
    };
};

/**
 * @brief A struct to store config parameters.
 */
struct config_params {
    int option;
    int num_tables;
    int num_columns[MAX_TABLES];
    /// The hostname of the server.
    char server_host[MAX_HOST_LEN];
    
    /// The listening port of the server.
    int server_port;
    
    /// The storage server's username
    char username[MAX_USERNAME_LEN];
    
    /// The storage server's encrypted password
    char password[MAX_ENC_PASSWORD_LEN];
    
    // Table names
    char tablelist[MAX_TABLES][MAX_TABLE_LEN];
    struct  column columnlist[MAX_TABLES][MAX_COLUMNS_PER_TABLE];
    
    /// The directory where tables are stored.
    //char data[MAX_PATH_LEN];
};

/*Custom struct*/

struct city{
	int counter;
    char name[MAX_KEY_LEN+1];//key
    int numocolumns;
    struct column columnlist[MAX_COLUMNS_PER_TABLE];//values
    struct city *next;
};

struct queryarg {
    char firstarg[MAX_COLUMNS_PER_TABLE][1024];
    char secondarg[MAX_COLUMNS_PER_TABLE][1024];
    char operator[MAX_COLUMNS_PER_TABLE];
    int max_keys;
};
/*End of custom struct*/

/**
 * @brief Exit the program because a fatal error occured.
 *
 * @param msg The error message to print.
 * @param code The program exit return value.
 */

static inline void die(char *msg, int code)
{
    printf("%s\n", msg);
    exit(code);
}

/**
 * @brief Keep sending the contents of the buffer until complete.
 * @return Return 0 on success, -1 otherwise.
 *
 * The parameters mimic the send() function.
 */
int sendall(const int sock, const char *buf, const size_t len);

/**
 * @brief Receive an entire line from a socket.
 * @return Return 0 on success, -1 otherwise.
 */
int recvline(const int sock, char *buf, const size_t buflen);

/**
 * @brief Read and load configuration parameters.
 *
 * @param config_file The name of the configuration file.
 * @param params The structure where config parameters are loaded.
 * @return Return 0 on success, -1 otherwise.
 */
int read_config(const char *config_file, struct config_params *params);

/**
 * @brief Generates a log message.
 * 
 * @param file The output stream
 * @param message Message to log.
 */
extern FILE *fileptr;//global file pointer variable
void logger(FILE *file, char *message);

/**
 * @brief Default two character salt used for password encryption.
 */
#define DEFAULT_CRYPT_SALT "xx"

/**
 * @brief Generates an encrypted password string using salt CRYPT_SALT.
 * 
 * @param passwd Password before encryption.
 * @param salt Salt used to encrypt the password. If NULL default value
 * DEFAULT_CRYPT_SALT is used.
 * @return Returns encrypted password.
 */
char *generate_encrypted_password(const char *passwd, const char *salt);

/*Custom functions*/
void cleanstring(char *string);
int decode_value(struct city *target, char *source);
int encode_value(struct column column[10], char *target, int limit);
int decode_line(char *received, char *command, char *tablename, char *keyname, char *value, int *counter);
int encode_line(char *type, char *status, char *statustwo, char *retline);
int encode_retval(struct column column[10], char *retval, int limit);
bool parser(int input, char type);
int find_index(char tablelist[MAX_TABLES][MAX_TABLE_LEN], char* name);
int columncopy(struct column *source, struct column *dest);
struct city* create_city(char* new_name, char *codedvalue);
//void insert_city(struct city *head, char* new_name, struct column new_column[MAX_COLUMNS_PER_TABLE]);
void insert_city(struct city *head, char *new_key, char *value_encoded); 
int delete_city(struct city **head, char* name);
struct city* find_city(struct city* head, char* name);
void print_city(struct city* new_city);
void print_list(struct city* head);
int findtableindex(char tables[MAXLEN][MAX_TABLE_LEN], char name[MAXLEN]);
void print_column(struct column *column);
void modify_city(struct city *head, char *value_encoded);
int query_argument(struct queryarg *querylist, char *values);
int query_compare(struct queryarg *querylist, struct city *target, int *querynum);
int query_write(char keylist[1000][1024], struct queryarg *querylist, struct city *head, int *limit, int *querynum);
//void parse_client(char *input, char *output);
void sget(char *s, int arraylength);
int check_column(struct config_params *param);
int argparse(char *in, char *out);
int queryparse(char *in, char *out);
int decode_queryret(char *ret_buffer, char **keylist);
void encode_queryret(int num_match, char keylist[1000][1024], char *retstring);
void add_equal(char *in);
/*End of custom functions*/

#endif
