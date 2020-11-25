#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include<time.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
	char mode[4];
	char partner[32];
} client_t;

//maurya
char qtype[2048];
char qtext[2048];
char qans[2048];
char qexplanation[2048];
char ans_out[2048];

int n_questions(char *filename){
    FILE *fp; 
    int count = 1; 
    char c; 
    fp = fopen(filename, "r");   
    if (fp == NULL) return 0;
    for (c = getc(fp); c != EOF; c = getc(fp)) if (c == '\n') count = count + 1; 
    fclose(fp);
    return count/6; 
} 
void get_question(int topic){
    
    char filename[100];

    // topic == 1: "Threads";
    // topic == 2: "Memory Management";
    // topic == 3: "Scheduling";    
    // as operating with strings (char *) in C is pathetic
    
    if(topic == 1) strcpy(filename,"Threads.txt");
    else if(topic == 2) strcpy(filename,"Memory Management.txt");
    else if(topic == 3) strcpy(filename,"Scheduling.txt"); 
    
    int max_questions = n_questions(filename);
    srand(time(0));
    int q_number = 1+ rand()%max_questions;

    FILE *file = fopen(filename, "r");

    int linecount = 1;

    char line[2048];

    while (fgets(line, sizeof line, file) != NULL) /* read a line */
    {
        if (linecount == 6*q_number-4) strcpy(qtype, line);
        else if (linecount == 6*q_number-3) strcpy(qtext, line);
        else if (linecount == 6*q_number-2) strcpy(qans, line);
        else if (linecount == 6*q_number-1){
            strcpy(qexplanation, line);
            break;
        };
        linecount++;
    }
    fclose(file);

    return;
}

void write_question(int topic, char * qtype, char * qtext, char * qans, char *qexplanation){
    char filename[100];

    // topic == 1: "Threads";
    // topic == 2: "Memory Management";
    // topic == 3: "Scheduling";    
    // as operating with strings (char *) in C is pathetic
    
    if(topic == 1) strcpy(filename,"Threads.txt");
    else if(topic == 2) strcpy(filename,"Memory Management.txt");
    else if(topic == 3) strcpy(filename,"Scheduling.txt"); 
    
    int n_old_questions = n_questions(filename);

    FILE *file = fopen(filename, "a");

    fprintf(file, "\n%d\n", n_old_questions+1);
    fprintf(file, "%s\n", qtype);
    fprintf(file, "%s\n", qtext);
    fprintf(file, "%s\n", qans);
    fprintf(file, "%s\n", qexplanation);
    fprintf(file, "question end\n");
    fclose(file);

    return;
}
//mauryaend

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//zaid
void send_to(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}
//zaidend

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void sendNames(int suid){
	int csock;
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == suid){
				csock=clients[i]->sockfd;
			}
		}
	}

	char tempo[50];
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != suid){
				sprintf(tempo, "%s: %d", clients[i]->name, clients[i]->uid);
				if(write(csock, tempo, strlen(tempo)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ];
	char name[32];
	char mode[4];
	char partner[32];
	int partner_uid;
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Name
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined\n", cli->name);
		printf("%s", buff_out);
		send_message(buff_out, cli->uid);
	}

	if(recv(cli->sockfd, mode, 4, 0) <= 0){
		printf("Didn't enter the mode.\n");
		leave_flag = 1;
	} else{
		strcpy(cli->mode, mode);
	}

	/*if(mode[0]=='2'){
		sendNames(cli->uid);
		if(recv(cli->sockfd, partner, 32, 0) <= 0 || strlen(partner) <  2 || strlen(partner) >= 32-1){
			printf("Didn't enter the partner.\n");
			leave_flag = 1;
		} else{
			strcpy(cli->partner, partner);
		}

	}*/

	bzero(buff_out, BUFFER_SZ);
	//zaid
	if(mode[0]=='2'){
	while(1){
		if (leave_flag) {
			break;
		}
		send_to("Type userId of person you want to connect to: \n Availible userIds are:\n",cli->uid);
		char buf1[32];
		for(int i=10;i<cli_count+10;i++){
			sprintf(buf1,"%d\n",i);
			send_to(buf1,cli->uid);
		}
		int rv=recv(cli->sockfd, partner, BUFFER_SZ, 0);
		partner_uid=atoi(partner);
		sprintf(buf1,"Incoming connection from %d\n", cli->uid);
		send_to(buf1,partner_uid);
		send_to("Choose Topic:\n1:Threads\n2:Memory Management\n3:Scheduling\nEnter 'exit' to exit program\n",cli->uid);
		send_to("Partner choosing topic please wait....\nEnter 'exit' to exit program\n",partner_uid);

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				//zaid
				if (buff_out[0]=='1')
				{
					get_question(1);
					send_to(qtext,cli->uid);
					send_to(qtext, partner_uid);
					int receive = recv(cli->sockfd, ans_out, BUFFER_SZ, 0);
					if(ans_out[0]==qans[0]) {
						send_to("Right!!\n",cli->uid);
						send_to(qexplanation,cli->uid);
						send_to("Your partner choose Right!!\n",partner_uid);
						send_to(qexplanation,partner_uid);
					}	
					else{
						send_to("Wrong:(\n",cli->uid);
						send_to(qexplanation,cli->uid);
						send_to("Your partner entered Wrong:(\n",partner_uid);
						send_to(qexplanation,partner_uid);
					}
				}
				else if (buff_out[0]=='2')
				{
					get_question(2);
					send_to(qtext,cli->uid);
					send_to(qtext, partner_uid);
					int receive = recv(cli->sockfd, ans_out, BUFFER_SZ, 0);
					if(ans_out[0]==qans[0]) {
						send_to("Right!!\n",cli->uid);
						send_to(qexplanation,cli->uid);
						send_to("Your partner choose Right!!\n",partner_uid);
						send_to(qexplanation,partner_uid);
					}	
					else{
						send_to("Wrong:(\n",cli->uid);
						send_to(qexplanation,cli->uid);
						send_to("Your partner entered Wrong:(\n",partner_uid);
						send_to(qexplanation,partner_uid);
					}
				}
				else if (buff_out[0]=='3')
				{
					get_question(3);
					send_to(qtext,cli->uid);
					send_to(qtext, partner_uid);
					int receive = recv(cli->sockfd, ans_out, BUFFER_SZ, 0);
					if(ans_out[0]==qans[0]) {
						send_to("Right!!\n",cli->uid);
						send_to(qexplanation,cli->uid);
						send_to("Your partner choose Right!!\n",partner_uid);
						send_to(qexplanation,partner_uid);
					}	
					else{
						send_to("Wrong:(\n",cli->uid);
						send_to(qexplanation,cli->uid);
						send_to("Your partner entered Wrong:(\n",partner_uid);
						send_to(qexplanation,partner_uid);
					}
				}
				

				//zaidend
				send_message(buff_out, cli->uid);
				str_trim_lf(buff_out, strlen(buff_out));
				//if(buff_out[0]=='z') send_message("entered ind mlode", 1);
				printf("%s: %s\n",cli->name, buff_out);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}	
	}
	//zaidend

	else if(mode[0]=='3'){
	while(1){
		if (leave_flag) {
			break;
		}
		send_to("Choose Topic:\n1:Threads\n2:Memory Management\n3:Scheduling\nEnter 'exit' to exit program\n",cli->uid);
		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		char qtype[32];
		char qtext[2048];
		char qans[32];
		char qexplanation[2048];
		if (receive > 0){
			if(strlen(buff_out) > 0){
				//zaid
				if (buff_out[0]=='1')
				{
					
					send_to("Enter question type:\n FIB\n MCQ\n",cli->uid);
					int receive1 = recv(cli->sockfd, qtype, 32, 0);
					send_to("Enter question:\n",cli->uid);
					int receive2 = recv(cli->sockfd, qtext, 2048, 0);
					send_to("Enter question answer:\n",cli->uid);
					int receive3 = recv(cli->sockfd, qans, 32, 0);
					send_to("Enter question Explanation:\n",cli->uid);
					int receive4 = recv(cli->sockfd, qexplanation, 2048, 0);
					write_question(1,qtype,qtext, qans,qexplanation);
					
				}
				else if (buff_out[0]=='2')
				{
					send_to("Enter question type:\n FIB\n MCQ\n",cli->uid);
					int receive1 = recv(cli->sockfd, qtype, 32, 0);
					send_to("Enter question:\n",cli->uid);
					int receive2 = recv(cli->sockfd, qtext, 2048, 0);
					send_to("Enter question answer:\n",cli->uid);
					int receive3 = recv(cli->sockfd, qans, 32, 0);
					send_to("Enter question Explanation:\n",cli->uid);
					int receive4 = recv(cli->sockfd, qexplanation, 2048, 0);
					write_question(2,qtype,qtext, qans,qexplanation);
				}
				else if (buff_out[0]=='3')
				{
					send_to("Enter question type:\n FIB\n MCQ\n",cli->uid);
					int receive1 = recv(cli->sockfd, qtype, 32, 0);
					send_to("Enter question:\n",cli->uid);
					int receive2 = recv(cli->sockfd, qtext, 2048, 0);
					send_to("Enter question answer:\n",cli->uid);
					int receive3 = recv(cli->sockfd, qans, 32, 0);
					send_to("Enter question Explanation:\n",cli->uid);
					int receive4 = recv(cli->sockfd, qexplanation, 2048, 0);
					write_question(3,qtype,qtext, qans,qexplanation);
				}
				

				//zaidend
				send_message(buff_out, cli->uid);
				str_trim_lf(buff_out, strlen(buff_out));
				//if(buff_out[0]=='z') send_message("entered ind mlode", 1);
				printf("%s: %s\n",cli->name, buff_out);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

	}
	
	else{
	while(1){
		if (leave_flag) {
			break;
		}
		send_to("Choose Topic:\n1:Threads\n2:Memory Management\n3:Scheduling\nEnter 'exit' to exit program\n",cli->uid);
		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				//zaid
				if (buff_out[0]=='1')
				{
					get_question(1);
					send_to(qtext,cli->uid);
					int receive = recv(cli->sockfd, ans_out, BUFFER_SZ, 0);
					if(ans_out[0]==qans[0]) {
						send_to("Right!!\n",cli->uid);
						send_to(qexplanation,cli->uid);
					}	
					else{
						send_to("Wrong:(\n",cli->uid);
						send_to(qexplanation,cli->uid);
					}
				}
				else if (buff_out[0]=='2')
				{
					get_question(2);
					send_to(qtext,cli->uid);
					int receive = recv(cli->sockfd, ans_out, BUFFER_SZ, 0);
					if(ans_out[0]==qans[0]) {
						send_to("Right!!\n",cli->uid);
						send_to(qexplanation,cli->uid);
					}	
					else{
						send_to("Wrong:(\n",cli->uid);
						send_to(qexplanation,cli->uid);
					}
				}
				else if (buff_out[0]=='3')
				{
					get_question(3);
					send_to(qtext,cli->uid);
					int receive = recv(cli->sockfd, ans_out, BUFFER_SZ, 0);
					if(ans_out[0]==qans[0]) {
						send_to("Right!!\n",cli->uid);
						send_to(qexplanation,cli->uid);
					}	
					else{
						send_to("Wrong:(\n",cli->uid);
						send_to(qexplanation,cli->uid);
					}
				}
				

				//zaidend
				send_message(buff_out, cli->uid);
				str_trim_lf(buff_out, strlen(buff_out));
				//if(buff_out[0]=='z') send_message("entered ind mlode", 1);
				printf("%s: %s\n",cli->name, buff_out);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}
	}

  /* Delete client from queue and yield thread */
	close(cli->sockfd);
  queue_remove(cli->uid);
  free(cli);
  cli_count--;
  pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;
  pthread_t tid;

  /* Socket settings */
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port = htons(port);

  /* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    return EXIT_FAILURE;
	}

	/* Bind */
  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Socket binding failed");
    return EXIT_FAILURE;
  }

  /* Listen */
  if (listen(listenfd, 10) < 0) {
    perror("ERROR: Socket listening failed");
    return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE QuizServer ===\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}
