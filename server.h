#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

struct account{
	char name[100];
	float balance;
	int in_service;
	pthread_mutex_t accLock;
};

struct bank{
	struct account acc[20];
	int numOfAccounts;
};

struct command{
	char action[100];
	char arg[100];
};

struct customerService{
	int in_session;
	int slot;
	struct account *acc;
};

typedef struct command *CMDline;
typedef struct bank *BK;
typedef struct customerService * cS;

#endif
