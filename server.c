#include "server.h"
#include "clientmenu.h"
#include <strings.h>

#define CLIENT_PORT 44331
#define SHMKEY 9876

pthread_attr_t kernel_attr;
sem_t systemSemaphore;
pthread_mutex_t accMutex;
BK banksystem;
size_t SIZEBANK = sizeof(struct bank);

void waitsignal(int signo){
	pid_t childPID;
	int status;

	if(signo == SIGCHLD){
		childPID = wait(&status);
		printf("Child Process %d exited with value %d!\n", childPID, WEXITSTATUS(status));
	}
}

void *client_action(void *csdptr){
	int csd = *(int *)csdptr;
	char request[1024];
	int reqSize;
	char answer[1024];
	cS session = malloc(sizeof(struct customerService));
	session->in_session = 0;
	session->acc = NULL;

	free(csdptr);
	//pthread_detach(pthread_self());
	
	printf("Connected with client!\n");
	
	/*test shm*/
	while( (reqSize = read(csd, request, sizeof(request))) > 0 ){
		request[reqSize] = '\0';
		bzero(answer, 1024);
		char *temp = malloc(sizeof(char) * 1024);
		bzero(temp, 1024);

		menu(tokenize(request), banksystem, session, temp, accMutex);
		
		/* make changes to customerService */
		if( strncmp(temp, "invalid", 7) == 0 ){
			strcpy(answer, "Wrong Command Format: <command> <argument>\n");
		}else if( strncmp(temp, "quit", 4) == 0 ){
			write(csd, "EXITonCOMMAND", 13);
			close(csd);
			break;
		}else if( strncmp(temp, "TryLocked", 9) == 0 ){
			int counter = 0;
			while( counter < 10 ){
				sleep(2);
				menu(tokenize(request), banksystem, session, temp, accMutex);
				if( strncmp(temp, "Success", 7) == 0 ){
					strcpy(answer, temp);
					answer[strlen(temp)] = '\0';
					break;
				}
				strcpy(answer, "Waiting to start Customer Session\n");
				write(csd, answer, strlen(answer)); 
				counter++;
			}			
			if( counter == 10 ){
				strcpy(answer, "Could not successfully log in\n");
			}
		}else{
			strcpy(answer, temp);
			answer[strlen(temp)] = '\0';
		}
		free(temp);
		
		write(csd, answer, strlen(answer));
	}
	close(csd);
	
	/* clean up */
	if( session->in_session == 1 ){
		logOutAccount(session);
	}
	printf("disconnected from client!\n");
	/*pthread_exit(0);*/
	exit(0);
}

void *session_acceptor(void *eww){
	/*pthread_t tid;*/
	int ssd;
	int csd;
	int *csdptr;
	struct sockaddr_in server;
	struct sockaddr_in client;
	socklen_t sSize = sizeof(struct sockaddr_in);
	int on = 1;
	struct sigaction cAction;
	
	pthread_detach(pthread_self());
	
	cAction.sa_handler = waitsignal;
	cAction.sa_flags = SA_RESTART;
	sigemptyset( &cAction.sa_mask );
	sigaction(SIGCHLD, &cAction, 0);

	/* create a socket */
	if( (ssd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
		printf("socket() failed\n");
		exit(0);
	}else if( setsockopt(ssd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1 ){
		printf("setting sockopt failed\n");
		exit(0);
	}

	/* set server sockaddr */
	memset( &server, 0, sSize);
	server.sin_family = AF_INET;
	server.sin_port = htons(CLIENT_PORT);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	/* bind */
	if( bind(ssd, (struct sockaddr *)&server, sizeof(server)) == -1 ){
		printf("bind failed\n");
		close(ssd);
		exit(0);
	}else if( listen(ssd, 20) == -1 ){
		printf("listen failed\n");
		close(ssd);
		exit(0);
	}else{
		while( (csd = accept(ssd, (struct sockaddr *)&client, &sSize)) != -1 ){
			csdptr = (int *) malloc(sizeof(int));
			*csdptr = csd;
			
		/* THIS IS THE MULTI PROCESSING ^^^^^^^^^^^^^^^^^^^^^^^*/	
			pid_t clientPid;
			clientPid = fork();			
			if( clientPid == 0 ){
				printf("Child Process %d started!\n", getpid());
				client_action(csdptr);
			}
				
		/* THIS IS THE MULTI THREADING ^^^^^^^^^^^^^^^^^^^^^^ */
		/*
			if( pthread_create(&tid, &kernel_attr, client_action, csdptr) != 0 ){
				printf("pthread_create client action failed\n");
				close(ssd);
				exit(0);
			}
		*/
		}
		close(ssd);
		pthread_exit(0);
	}
}

void report_handler(int signo){
	if( signo == SIGALRM ){
		sem_post( &systemSemaphore );
	}
}

void report(){
        int numAcc = banksystem->numOfAccounts;
        int x;
        struct account bAc;

        printf("*****SYSTEM REPORT*****\n\n");
        printf("NAME\t\t\tBALANCE\t\tINSERVICE\n");
        printf("----\t\t\t-------\t\t---------\n\n");
        for( x=0; x<numAcc; x++ ){
                bAc = banksystem->acc[x];
                printf("%s\t\t", bAc.name);
		if( strlen(bAc.name) < 7 ){
			printf("\t");
		}
                printf("%0.2f\t\t", bAc.balance);
                if( bAc.in_service == 1 ){
                        printf("YES\n");
                }else{
                        printf("NO\n");
                }
        }
        printf("\n*****END OF REPORT*****\n");
}

void *bank_report(void *ignore){
	struct sigaction action;
	struct itimerval interval;
	
	pthread_detach( pthread_self() );
	action.sa_flags = SA_SIGINFO | SA_RESTART;
	action.sa_handler = report_handler;
	sigemptyset( &action.sa_mask );
	sigaction( SIGALRM, &action, 0);

	interval.it_interval.tv_sec = 20;
	interval.it_interval.tv_usec = 0;
	interval.it_value.tv_sec = 20;
	interval.it_value.tv_usec = 0;
	setitimer( ITIMER_REAL, &interval, 0 );

	while(1){
		sem_wait( &systemSemaphore );
		pthread_mutex_lock( &accMutex );
		report();
		pthread_mutex_unlock( &accMutex );
		sched_yield();
	}
}

int main(int argc, char **argv){
	pthread_t tid;
	key_t key = SHMKEY;
	int shmid;
	
	/* initalize shared memory */
	if( (shmid = shmget(key, SIZEBANK, IPC_CREAT | 0666)) < 0 ){
		printf("shmget() failed\n");
		exit(0);
	}else{
		banksystem = shmat(shmid, NULL, 0);
		banksystem->numOfAccounts = 0;
	}
	/* initialize threads */
	if( pthread_attr_init( &kernel_attr ) != 0 ){
		printf("pthread_attr_init() failed\n");
		exit(0);
	}else if( pthread_attr_setscope( &kernel_attr, PTHREAD_SCOPE_SYSTEM ) != 0 ){
		printf("pthread_attr_setscope() failed\n");
		exit(0);
	}else if( sem_init(&systemSemaphore, 0, 0) != 0 ){
		printf("sem_init failed\n");
		exit(0);
	}else if( pthread_mutex_init(&accMutex, NULL) != 0 ){
		printf("mutex_init failed\n");
		exit(0);
	}else if( pthread_create( &tid, &kernel_attr, session_acceptor, NULL) != 0 ){
		printf("pthread_create session failed\n");
		exit(0);
	}else if( pthread_create( &tid, &kernel_attr, bank_report, NULL) != 0 ){
		printf("pthread_create report failed\n");
		exit(0);
	}else{
		printf("server is waiting to receive clients...\n");
		pthread_exit(0);
	}	
}
