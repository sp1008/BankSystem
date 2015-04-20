#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <netdb.h>


#define CLIENT_PORT 44331

pthread_attr_t kernel_attr;
pthread_mutex_t mutexLock;


void *writer(void *wsd){
	int sd = *(int *)wsd;
	char string[1024];
	ssize_t length;

	free(wsd);
	pthread_detach(pthread_self());
	
	while(1){
		printf("COMMAND>>");
		fflush(stdout);
		length = read(0, string, sizeof(string));
		string[length-1] = '\0';
		write(sd, string, strlen(string));
		bzero(string, 1024);
		sleep(2);
	}
	pthread_exit(0);
}

void *reader(void *lsd){
	int sd = *(int *)lsd;
	char buffer[1024];
	ssize_t length;

	free(lsd);
	pthread_detach(pthread_self());
	
	while(1){
		length = read(sd, buffer, sizeof(buffer));
		if( length == 0 ){
			break;
		}
		buffer[length] = '\0';
		if( strncmp(buffer, "EXITonCOMMAND", 13) == 0 ){
			break;
		}
		printf("Received: %s\n", buffer);
	}
	close(sd);
	printf("Disconnected from client\n");
	exit(0);
}

int connector(const char *name){
	int sd;	
	char *port = "44331";
	struct addrinfo addrinfo;
	struct addrinfo *result;
	
	addrinfo.ai_flags = 0;
	addrinfo.ai_family = AF_INET;
	addrinfo.ai_socktype = SOCK_STREAM;
	addrinfo.ai_protocol = 0;
	addrinfo.ai_addrlen = 0;
	addrinfo.ai_addr = NULL;
	addrinfo.ai_canonname = NULL;
	addrinfo.ai_next = NULL;

	if( getaddrinfo( name, port, &addrinfo, &result ) != 0 ){
		printf("getaddrinfo failed\n");
		return -1;
	}else if( (sd = socket( result->ai_family, result->ai_socktype, result->ai_protocol )) == -1 ){
		printf("socket failed\n");
		freeaddrinfo( result );
		return -1;
	}
	
	do{
		if( (errno=0), connect( sd, result->ai_addr, result->ai_addrlen ) == -1 ){
			sleep(3);
			printf("Trying to reach server... \n");
		}else{
			printf("Connected with server!\n");
			freeaddrinfo(result);
			return sd;
		}
	}while( errno == ECONNREFUSED );
	
	freeaddrinfo( result );
	return -1;	
}

int main( int argc, char ** argv){
	pthread_t tid;
	int sd;

	if( argc !=  2){
		printf("You must declare server name!\n");
		exit(0);
	}else if( pthread_attr_init( &kernel_attr ) != 0 ){
		printf("pthread_attr_init failed\n");
		exit(0);
	}else if( pthread_attr_setscope( &kernel_attr, PTHREAD_SCOPE_SYSTEM ) != 0 ){
		printf("pthread_set_scope failed\n");
		exit(0);
	}else if( pthread_mutex_init(&mutexLock, NULL) != 0 ){
		printf("mutex_init failed\n");
		exit(0);
	}else if( (sd = connector(argv[1])) == -1 ){
		printf("connector failed\n");
		exit(0);
	}else{
		int *wsd = (int *) malloc(sizeof(int));
		int *lsd = (int *) malloc(sizeof(int));
		*wsd = sd;
		*lsd = sd;
		
		if( pthread_create(&tid, &kernel_attr, writer, wsd) != 0 ){
			printf("pthread_create failed\n");
			exit(0);
		}else if( pthread_create(&tid, &kernel_attr, reader, lsd) != 0 ){
			printf("pthread_create failed\n");
			exit(0);
		}
		/*	
		int length;
		char string[1024];
		char buffer[1024];
	
		while( write(1, "\nTYPE>>", 7), (length = read(0, string, sizeof(string))) > 0 ){
                        string[length-1] = '\0';
                        printf("Sending to server: %s\n", string);
                        printf("string length: %d\n", strlen(string));
                        write( sd, string, strlen(string));
                        length = read( sd, buffer, sizeof(buffer) );
                        buffer[length] = '\0';
                        if( strncmp(buffer, "EXITonCOMMAND", 13) == 0 ){
                                close(sd);
                                break;
                        }
                        printf("Received from server: %s", buffer);
			bzero(string, 1024);
			bzero(buffer, 1024);
                }
                printf("Disconnected from server %s.\n", argv[1]);
		*/
        }
	pthread_exit(0);
}
