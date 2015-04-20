#include "server.h"
#include "clientmenu.h"

/* FOLLOWING FORMAT
 * <command>_<argument> '_' represents space
 */

CMDline tokenize(char *request){
	int x = 0;
	int y = 0;
	CMDline newCommand = malloc(sizeof(struct command));
	int reqSize = strlen(request);
	
	while(request[x] != ' ' && x < reqSize){
		newCommand->action[x] = tolower(request[x]);
		x++;
	}	
	newCommand->action[x] = '\0';

	/* if there is a space in the beginning OR no commands */
	if( x == 0 ){
		return NULL;
	}
	
	while(request[x] == ' ' && x < reqSize){
		x++;
	}
	
	while( x < reqSize ){
		newCommand->arg[y] = tolower(request[x]);
		y++;
		x++;
	}
	newCommand->arg[y] = '\0';

	return newCommand;
}

void menu( CMDline newCommand, BK banksystem, cS session, char *temp, pthread_mutex_t accMutex ){
	char *error = malloc(sizeof(char) * 300);
	char answer[1024];
	answer[0] = '\0';	
	if( newCommand == NULL ){
		temp = NULL;
	}else if( strcmp(newCommand->action, "create") == 0 ){		
		if( session->in_session == 0 ){
			pthread_mutex_lock( &accMutex );
			makeNewAccount(newCommand->arg, banksystem, error);
			pthread_mutex_unlock( &accMutex );
			if( error[0] == 'S' ){
				strcpy(answer, "Success! Account has been made! Please log in using 'serve'\n");
			}else{
				strcpy(answer, "Failed to make an account - ");
				strcat(answer, error);
			}
		}else{
			strcpy(answer, "You must log out of your current account first.\n");
		}
	}else if( strcmp(newCommand->action, "serve") == 0 ){
		if( session->in_session == 0){
			pthread_mutex_lock( &accMutex );
			int logIn = findAccount(newCommand->arg, banksystem, session);
			pthread_mutex_unlock( &accMutex );
			if( logIn == 1 ){
				strcpy(answer, "Successfully Logged In!\n");
			}else if( logIn == 2 ){
				strcpy(answer, "TryLocked");
			}else{
				strcpy(answer, "Could not find the specified account\n");
			}
		}else{
			strcpy(answer, "You are already logged in!\n");
		}
	}else if( strcmp(newCommand->action, "deposit") == 0 || strcmp(newCommand->action, "withdraw") == 0 ){
		if( session->in_session == 1 ){
			pthread_mutex_lock( &accMutex );
			changeAmount(newCommand->arg, newCommand->action, session, error);
			pthread_mutex_unlock( &accMutex );
			if( error[0] != 'E' ){
				strcpy(answer, "Successful: You ");
				strcat(answer, error);
			}else{
				strcpy(answer, "Failed- ");
				strcat(answer, error);			
			}
		}else{
			strcpy(answer, "You must log in to an account first.\n");
		}
	}else if( strcmp(newCommand->action, "query") == 0 ){
		if( session->in_session == 1 ){
			pthread_mutex_lock( &accMutex );
			getAmount(newCommand->arg, session, error);
			pthread_mutex_unlock( &accMutex );
			if( error[0] != 'E' ){
				strcpy(answer, "The current balance is $");
				strcat(answer, error);				
			}else{
				strcpy(answer, "Failed to get amount - ");
				strcat(answer, error);
			}
		}else{
			strcpy(answer, "You must log in to an account first.\n");
		}
	}else if( strcmp(newCommand->action, "end") == 0 ){
		if( session->in_session == 1 ){
			logOutAccount(session);
			strcpy(answer, "Successfully logged out\n");
		}else{
			strcpy(answer, "You must log in to an account first.\n");
		}
	}else if( strcmp(newCommand->action, "quit") == 0 ){
		if( session->in_session == 1 ){
			logOutAccount(session);
		}
		strcpy(answer, "quit");
	}else{
		strcpy(answer, "invalid");
	}

	strcpy(temp, answer);
	temp[strlen(answer)] = '\0';
	free(error);
	free(newCommand);
}

void makeNewAccount(char *arg, BK banksystem, char *error){
	int numberAccounts = banksystem->numOfAccounts;
	int x;

	if( numberAccounts >= 20 ){
		strcpy(error, "ERROR: Account slots all full.\n");
		return;
	}

	if( !isValidName(arg) ){	
		strcpy(error, "ERROR: Invalid name.\n Minimum 4 alphanumeric characters, max 2 spaces (less than 100 characters)\n");
		return;
	}

	/* check for repeating names */
	for( x=0; x < banksystem->numOfAccounts; x++ ){
		if( strcmp(banksystem->acc[x].name, arg) == 0 ){
			strcpy(error, "ERROR: Name already in use. Please select different name\n");
			return;
		}
	}	

	if( strncpy(banksystem->acc[numberAccounts].name, arg, 100) == NULL ){
		strcpy(error, "ERROR: Couldn't make this account - name\n");
		return;
	}

	if( pthread_mutex_init(&banksystem->acc[numberAccounts].accLock, 0) != 0 ){
		strcpy(error, "ERROR: Locking Mechanism failed\n");
	}
	
	banksystem->acc[numberAccounts].balance = 0;
	banksystem->acc[numberAccounts].in_service = 0;
	
	banksystem->numOfAccounts++;
	strcpy(error, "SUCCESS\n");
	return;
}

int findAccount(char *arg, BK banksystem, cS session){
	int x;
	int numberAccounts = banksystem->numOfAccounts;

	for( x=0; x < numberAccounts; x++ ){
		if( strcmp(banksystem->acc[x].name, arg) == 0 ){
			if( pthread_mutex_trylock(&banksystem->acc[x].accLock) == 0 ){
				banksystem->acc[x].in_service = 1;
				session->acc = &banksystem->acc[x];
				session->slot = x;
				session->in_session = 1;
				return 1;
			}else{
				return 2;
			}
		}
	}

	return 0;
}

void changeAmount(char *arg, char *action, cS session, char *error){
	if( !isValidFloat(arg) ){
		strcpy(error, "ERROR: wrong input format- Need float (max 2 decimal points) for argument\n");
		return;
	}

	float amount = atof(arg);	
	
	if( action[0] == 'd' ){
		session->acc->balance += amount;
		strcpy(error, "deposited");
	}else{
		if( session->acc->balance < amount ){
			strcpy(error, "ERROR: You cannot withdraw more than you have.\n");
			return;
		}
		session->acc->balance -= amount;
		strcpy(error, "withdrew");
	}

	strcat(error, " $");
	strcat(error, arg);
	strcat(error, "\n");
	return;
}

void getAmount(char *arg, cS session, char *error){
	if( strlen(arg) >= 1 ){
		strcpy(error, "ERROR: Input '");
		strcat(error, arg);
		strcat(error, "' is unrecognized.\n");
		return;
	}
	
	float amount = session->acc->balance;

	if(amount == 0){
		strcpy(error, "0.00\n");
		return;
	}

	sprintf(error, "%0.2f", amount);
	strcat(error, "\n");	

	return;
}

int logOutAccount(cS session){
	pthread_mutex_unlock(&session->acc->accLock);
	session->acc->in_service = 0;
	session->acc = NULL;
	session->in_session = 0;
	return 1;
}

int isValidName(char *name){
	int x;
	int length = strlen(name);
	int countspace = 0;	

	if( length < 4 || length >= 100 ){
		return 0;
	}

	for( x=0; x<length; x++ ){
		if( isalnum(name[x]) ){
			continue;
		}else{
			if( name[x] == ' ' && countspace < 2 ){
				countspace++;
				continue;
			}else{
				return 0;
			}
		}
	}
	
	return 1;
}

int isValidFloat(char *num){
	int x;
	int length = strlen(num);

	if( length < 1){
		return 0;
	}

	/* You cannot have first character 0 without a decimal point next */	
	if( num[0] == '0' && num[1] != '.' ){
		return 0;
	}

	for( x=0; x<length; x++ ){
		if( isdigit(num[x]) ){
			continue;
		}else{
			if( num[x] == '.' ){
				/* only two integers allowed after . */
				int y = length-x;
				if( y > 3 || y < 2 ){
					return 0;
				}else{
					y = y-1;
					x = x+1;
					while( y > 0 ){
						if( isdigit(num[x]) ){
							x++;
							y--;
							continue;
						}else{
							return 0;
						}
					}
					break;
				}
			}else{
				return 0;
			}
		}
	}
	return 1;
}



