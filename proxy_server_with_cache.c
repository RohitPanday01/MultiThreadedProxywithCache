#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include<signal.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAX_BYTES 4096    //max allowed size of request/response
#define MAX_CLIENTS 10     //max number of client requests served at a time
#define MAX_SIZE 200*(1<<20)     //size of the cache
#define MAX_ELEMENT_SIZE 10*(1<<20)     //max size of an element in cache
 

//when you define a struct with a pointer to its own type, you need to prefix the type name with the keyword struct unless you create a typedef for it.
typedef struct cache_element cache_element;  //typedef is a keyword used to create a new name (alias) for an existing type.

struct cache_element{
    char * data ;
    int len ;
    char* url;
    time_t lru_time_track;
    cache_element* next; 
};

cache_element* find(char* url);
int add_cache_element(char* data ,int size, char* url);
void remove_cache_element();

int port_number = 8080;
int proxy_socketId;					
pthread_t tid[MAX_CLIENTS];         
sem_t seamaphore;	                		       
pthread_mutex_t lock;               //lock is used for locking the cache


cache_element* head;                //pointer to the cache
int cache_size;            


int sendErrorMessage(int socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

	switch(status_code)
	{
		case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
				  printf("400 Bad Request\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
				  printf("403 Forbidden\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
				  printf("404 Not Found\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
				  //printf("500 Internal Server Error\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
				  printf("501 Not Implemented\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
				  printf("505 HTTP Version Not Supported\n");
				  send(socket, str, strlen(str), 0);
				  break;

		default:  return -1;

    }
    return 1;
}
int connectRemoteServer(char * host_addr , int port_num){

    int remoteSocket = socket(AF_INET , SOCK_STREAM , 0);

    if(remoteSocket <  0){
        printf("ERROR IN CREATING SOCKET");
        return -1;
    }

    struct hostent *host = gethostbyname(host_addr);	
	if(host == NULL)
	{
		fprintf(stderr, "No such host exists.\n");	
		return -1;
	}

    struct sockaddr_in server_addr;

    bzero((char*)&server_addr , sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);

    bcopy((char *)host->h_addr,(char *)&server_addr.sin_addr.s_addr,host->h_length);

    if(connect(remoteSocket , (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0){
        fprintf(stderr, "Error Connecting ");
        return -1;
    }

    return remoteSocket;

}


int handleRequest(int clientSocket , struct ParsedRequest *req , char* tempReq ){
    char * buf = (char *)malloc(MAX_BYTES*sizeof(char));
    strcpy(buf, "GET ");
    strcat(buf , req->path );
    strcat(buf , " ");
    strcat(buf , req->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);

    if(ParsedHeader_set(req, "Connection" , "close") < 0){
        printf("set Header not working\n");
    }

    if(ParsedHeader_get(req, "Host") == NULL){
        if(ParsedHeader_set(req , "Host", req->host) < 0){
            printf("Set \"Host\" Header Key Not working ");
        }
    }

    if(ParsedRequest_unparse_headers(req , buf + len  , (size_t)MAX_BYTES-len) < 0){
        printf("Unparsed Failed");
    }

    int server_port = 80;   // default server port 
    if(req->port != NULL){
        server_port = atoi(req->port);
    }

    int remoteSocketId = connectRemoteServer(req->host , server_port);
    if(remoteSocketId < 0){
        return -1;
    }

    int bytes_send = send(remoteSocketId, buf, strlen(buf) , 0);
    bzero(buf, MAX_BYTES);

    bytes_send = recv(remoteSocketId, buf, MAX_BYTES -1 , 0);
    char * temp_buffer = (char* )malloc(sizeof(char)*MAX_BYTES);
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_indx = 0;

    while(bytes_send > 0 ){
        bytes_send = send(clientSocket , buf, bytes_send , 0);

        for(int i = 0; i < bytes_send/sizeof(char); i++){
            temp_buffer[temp_buffer_indx] = buf[i];
            temp_buffer_indx++;
        }
        temp_buffer_size += MAX_BYTES;
		temp_buffer=(char*)realloc(temp_buffer,temp_buffer_size);

		if(bytes_send < 0)
		{
			perror("Error in sending data to client socket.\n");
			break;
		}
		bzero(buf, MAX_BYTES);

		bytes_send = recv(remoteSocketId, buf, MAX_BYTES-1, 0);
    }

    temp_buffer[temp_buffer_indx] = '\0';
    free(buf);
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq);
    printf("Done\n");
	free(temp_buffer);
	
 	close(remoteSocketId);
	return 0;

}

int checkHTTPversion(char *msg)
{
	int version = -1;
	if(strncmp(msg, "HTTP/1.1", 8) == 0)
	{
		version = 1;
	}
	else if(strncmp(msg, "HTTP/1.0", 8) == 0)			
	{
		version = 1;						
	}
	else
		version = -1;
	return version;
}

void* thread_fn(void* socketNew) {
    sem_wait(&seamaphore);
    int p;
    sem_getvalue(&seamaphore, &p);
    printf("Semaphore value: %d\n", p);

    int *t = (int *)(socketNew);
    int socket = *t;
    int bytes_send_client;

    char *buffer = (char *)calloc(MAX_BYTES, sizeof(char));
    if (!buffer) {
        perror("Memory allocation failed for buffer");
        return NULL;
    }

    bytes_send_client = recv(socket, buffer, MAX_BYTES, 0);

    // Receive all data into buffer
    while (bytes_send_client > 0) {
        int len = strlen(buffer);

        if (strstr(buffer, "\r\n\r\n") == NULL) {
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        } else {
            break;
        }
    }

    char *tempReq = strdup(buffer);
    if (!tempReq) {
        perror("Memory allocation failed for tempReq");
        free(buffer);
        return NULL;
    }

    struct cache_element *temp = find(tempReq);
    if (temp != NULL) {
        int size = temp->len / sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];

        while (pos < size) {
            bzero(response, MAX_BYTES);
            for (int i = 0; i < MAX_BYTES && pos < size; i++) {
                response[i] = temp->data[pos++];
            }
            int total_sent = 0;
            while (total_sent < MAX_BYTES) {
                int sent = send(socket, response + total_sent, MAX_BYTES - total_sent, 0);
                if (sent < 0) {
                    perror("Error in sending data to client socket");
                    break;
                }
                total_sent += sent;
            }
        }
        printf("Data retrieved from the Cache\n\n%s\n\n", response);
    } else if (bytes_send_client > 0) {
        struct ParsedRequest *req = ParsedRequest_create();
        if (!req) {
            perror("Failed to create ParsedRequest");
            free(buffer);
            free(tempReq);
            return NULL;
        }

        if (ParsedRequest_parse(req, buffer, bytes_send_client) < 0) {
            printf("Parsing failed\n");
        } else {
            bzero(buffer, MAX_BYTES);
            if (!strcmp(req->method, "GET")) {
                if (req->host && req->path && checkHTTPversion(req->version)) {
                    bytes_send_client = handleRequest(socket, req, tempReq);
                    if (bytes_send_client == -1) {
                        sendErrorMessage(socket, 500);
                    }
                } else {
                    sendErrorMessage(socket, 500);
                }
            } else {
                printf("This server only supports GET requests\n");
            }
        }
        ParsedRequest_destroy(req);
    } else if (bytes_send_client < 0) {
        perror("Error in receiving from client");
    } else if (bytes_send_client == 0) {
        printf("Client disconnected!\n");
    }

    if (shutdown(socket, SHUT_RDWR) < 0) {
        perror("Error shutting down socket");
    }
    close(socket);
    free(buffer);
    sem_post(&seamaphore);

    sem_getvalue(&seamaphore, &p);
    printf("Semaphore post value: %d\n", p);

    free(tempReq);
    return NULL;
}

int main(int argc , char * argv[]){

    int client_socketId , client_len;
    struct sockaddr_in client_addr , server_addr;

    sem_init(&seamaphore , 0 , MAX_CLIENTS);
    pthread_mutex_init(&lock , NULL);


    if(argc == 2){
        port_number = atoi(argv[1]);
    }else{
        printf("Too Few Arguments");
        exit(1);
    }

    printf("Setting Proxy Server Port : %d\n", port_number);

    if((proxy_socketId = socket(AF_INET, SOCK_STREAM , 0)) == -1){
        perror("server :socket ");
        exit(1);
    }

    int reuse  = 1;
    if(setsockopt(proxy_socketId, SOL_SOCKET ,SO_REUSEADDR , (const char*)&reuse, sizeof(reuse)) < 0 ){
        perror("setsockopt(SO_REUSEADDR) failed\n");
    }

    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(proxy_socketId , (struct sockaddr *)&server_addr ,sizeof(server_addr) ) < 0 ){
        perror("Port is not free\n");
		exit(1);
    }

    printf("Binding on port: %d\n",port_number);

    if(listen(proxy_socketId , MAX_CLIENTS) == -1){
        perror("listen");
        exit(1);
    }

    int i = 0;
    int Connected_socketId[MAX_CLIENTS];


    while(1){
        memset((char *)&client_addr, 0, sizeof(client_addr));
        client_len = sizeof(client_addr);

        client_socketId = accept(proxy_socketId , (struct sockaddr*)&client_addr,(socklen_t*)&client_len );

        if(client_socketId < 0){
            fprintf(stderr ,"Error in Accepting connection \n");
            exit(1);
        }else{
            Connected_socketId[i] = client_socketId;
        }

		char str[INET_ADDRSTRLEN];							// INET_ADDRSTRLEN: Default ip address size
		inet_ntop( AF_INET, &((struct sockaddr_in *)&client_addr)->sin_addr, str, INET_ADDRSTRLEN );
        printf("Client is connected with port number: %d and ip address: %s \n",ntohs(client_addr.sin_port), str);

        //creating thread for each client accepted 
        pthread_create(&tid[i] , NULL ,thread_fn, (void*)&Connected_socketId[i] );
        i++;

    }
    close(proxy_socketId);									
 	    return 0;
}


cache_element* find(char* url){
    
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("find cache lock locked %d\n" ,temp_lock_val);
    cache_element* site = NULL;
    
    if(head != NULL){
        site = head;
        while(site != NULL){
            if(!strcmp(site->url, url)){
                printf("LRU time Track before: %ld" ,site->lru_time_track );
                site->lru_time_track = time(NULL);
                printf("LRU time Track Before: %ld" ,site->lru_time_track);

            }
            site = site->next;
        }
    }
    else {
    printf("\nurl not found\n");
	}
	//sem_post(&cache_lock);
    temp_lock_val = pthread_mutex_unlock(&lock);
	printf("Find Cache Lock Unlocked %d\n",temp_lock_val); 
    return site;

}
void remove_cache_element(){
    cache_element * prev ;
    cache_element* nex ;
    cache_element*temp;

    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove Cache Lock Acquired %d\n",temp_lock_val);
    if(head != NULL){
       for (nex = head, prev = head, temp =head ; nex -> next != NULL; 
			 nex = nex -> next) { 
			if(( (nex -> next) -> lru_time_track) < (temp -> lru_time_track)) {
				temp = nex -> next;
				prev = nex;
			}
		}
        if(temp == head){
            head = head->next;
        }else{
            prev->next = temp->next;
        }

        cache_size = cache_size - (temp->len)  - sizeof(cache_element) - strlen(temp->url) - 1;
        free(temp->data);
        free(temp->url);
        free(temp);
    }
    temp_lock_val = pthread_mutex_unlock(&lock);
	printf("Remove Cache Lock Unlocked %d\n",temp_lock_val); 
}

int add_cache_element(char* data, int size, char* url) {
    int temp_lock_val = pthread_mutex_lock(&lock);
    if (temp_lock_val != 0) {
        printf("Mutex lock failed with error code %d\n", temp_lock_val);
        return 0; // Handle lock failure
    }
    printf("Add Cache Lock Acquired %d\n", temp_lock_val);

    int element_size = sizeof(cache_element) + size + 1 + strlen(url);

    if (element_size > MAX_ELEMENT_SIZE) {
        printf("Element size is greater than MAX_ELEMENT_SIZE\n");
        pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked\n");
        return 0;
    } else {
        // Remove cache elements if the new element exceeds the cache size
        while (cache_size + element_size > MAX_SIZE) {
            remove_cache_element();
        }

        cache_element* element = (cache_element*)malloc(sizeof(cache_element));
        if (!element) {
            perror("Memory allocation failed for cache element");
            pthread_mutex_unlock(&lock);
            return 0;
        }

        element->data = (char*)malloc(size + 1);
        if (!element->data) {
            perror("Memory allocation failed for cache data");
            free(element);
            pthread_mutex_unlock(&lock);
            return 0;
        }
        strcpy(element->data, data);

        element->url = (char*)malloc(strlen(url) + 1);
        if (!element->url) {
            perror("Memory allocation failed for cache URL");
            free(element->data);
            free(element);
            pthread_mutex_unlock(&lock);
            return 0;
        }
        strcpy(element->url, url);

        element->lru_time_track = time(NULL);
        element->next = head;
        head = element;

        cache_size += element_size;

        pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked\n");
        return 1;
    }
}






