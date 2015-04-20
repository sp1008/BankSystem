#ifndef CLIENTMENU_H
#define CLIENTMENU_H

#include "server.h"


CMDline tokenize(char *);
void menu(CMDline, BK, cS, char *, pthread_mutex_t);
void makeNewAccount(char *, BK, char *);
int findAccount(char *, BK, cS);
void changeAmount(char *, char *, cS, char *);
void getAmount(char *, cS, char *);
int logOutAccount(cS);
int isValidName(char *);
int isValidFloat(char *);

#endif
