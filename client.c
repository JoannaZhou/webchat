#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
 
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
 
#define SERVERIP "127.0.0.1"
#define SERVERPORT 8080
 
#define MAX_DATA 1024
#define MAX_NAME 32
#define OPTLEN 16
#define LINEBUF 2048
#define MAX_SESSIONS 10
#define MAX_PASSWORD 50
#define MAX_CLIENTS 10
#define MAX_SESSIONS 10
 
#define LOGIN 100
#define LO_ACK 101
#define LO_NAK 102
#define EXIT 200
#define JOIN 300
#define JN_ACK 301
#define JN_NAK 302
#define LEAVE_SESS 400
#define NEW_SESS 401
#define NS_ACK 402 
#define NS_NAK 403
#define MESSAGE 500
#define QUERY 600
#define QU_ACK 601
#define QUIT 700
 	 

struct lab3message {
    unsigned int type; // instruction 
    unsigned int size; // length of data
    unsigned char source[MAX_NAME]; // client's alias
    unsigned char data[MAX_DATA]; // payload
};
 
struct USER {
        int sockfd; // user's socket descriptor
        char alias[MAX_NAME]; // user's name
	char password[MAX_PASSWORD];
	char sessions[MAX_SESSIONS];
};
 
struct THREADINFO {
    pthread_t thread_ID; // thread's pointer
    int sockfd; // socket file descriptor
};

int isconnected, sockfd, insession = 0;
struct USER me;

int connect_with_server(char* server_IP,int server_port);
void logout(struct lab3message *pack_data, struct USER *me);
void login(struct lab3message *pack__data, struct USER *me, char* server_IP, char* server__port);
void *receiver(void *param);
void sendtoall(struct lab3message *pack_data, struct USER *me);
void join_session(struct lab3message *pack_data, struct USER *me);
void leave_session (struct lab3message *pack_data, struct USER *me);
void create_session(struct lab3message *pack_data, struct USER *me); 
void list(struct lab3message *pack_data, struct USER *me);



int main(int argc, char **argv) {

	int sockfd;
	char command[LINEBUF];
	
	//struct USER me;
	struct lab3message pack_data;

	memset(&me, 0, sizeof(struct USER));
	memset(&pack_data, 0, sizeof(struct lab3message));
	char* endcommand;

    while(gets(command)) {
	//printf("In main while loop\n");
	char* command_type = strtok_r(command, " ", &endcommand); //identify type

        if (!strcmp(command_type, "/login")) {
		char* client_ID = strtok_r(NULL, " ", &endcommand);
		char* password = strtok_r(NULL, " ", &endcommand);
		char* server_IP = strtok_r(NULL, " ", &endcommand);
		char* server_port = strtok_r(NULL, "\n", &endcommand);
		//printf("The server port here is %s\n", server_port);
		//int server_port = atoi(server__port);
		//lab3message fill in
		pack_data.type = LOGIN;
		strcpy(pack_data.source, client_ID);
		snprintf(pack_data.data, LINEBUF, "%s,%s", pack_data.source, password);
		pack_data.size = strlen(pack_data.data);
		
		//user struct fill in
		strcpy(me.alias,client_ID);
		strcpy(me.password,password);
         
		//int success = 0;
		login(&pack_data, &me, server_IP, server_port);
        }

	else if(!strcmp(command_type, "/logout")) {

		//lab3message fill in
		pack_data.type = EXIT;
		strcpy(pack_data.source,me.alias);
		
		logout(&pack_data, &me);
		
        }

	else if (!strcmp(command_type, "/joinsession")) {
		
		char* session_ID = strtok_r(NULL, " ", &endcommand);

		//lab3message fill in
		pack_data.type = JOIN;
		strcpy(pack_data.source , me.alias);
		snprintf(pack_data.data, LINEBUF, "%s", session_ID);
		pack_data.size = strlen(pack_data.data);
		//printf("Before join function in main\n");
		join_session(&pack_data, &me);
		//printf("After join function in main\n");

	}

	else if (!strcmp(command_type, "/leavesession")) {

		//lab3message fill in
		pack_data.type = LEAVE_SESS;
		strcpy(pack_data.source , me.alias);

		leave_session(&pack_data, &me);

	}

	else if (!strcmp(command_type, "/createsession")) {

		char* session_ID = strtok_r(NULL, " ", &endcommand);

		//lab3message fill in
		pack_data.type = NEW_SESS;
		strcpy(pack_data.source , me.alias);
		snprintf(pack_data.data, LINEBUF, "%s", session_ID);
		pack_data.size = strlen(pack_data.data);

		create_session(&pack_data, &me);

	}

	else if (!strcmp(command_type, "/list")) {
		
		//lab3message fill in
		pack_data.type = QUERY;
		strcpy(pack_data.source , me.alias);

		list(&pack_data, &me);

	}
	
	else if (!strcmp(command_type, "/quit")) {
		//lab3message fill in
		pack_data.type = QUIT;
		strcpy(pack_data.source , me.alias);

		logout(&pack_data, &me);
		break;
	}


	else if ( isconnected && insession ){ //sending or receiving message to and from all
		/*
		//casual checking for messages
		struct lab3message packet;
		memset(&packet, 0, sizeof(struct lab3message));
		int rec = recv(sockfd, (void *)&packet, sizeof(struct lab3message), 0);
		*/
		
		char* message = strtok_r(NULL, "\n", &endcommand);
		//printf("What I'm strtok-ing is: %s\n", message);
		
		//lab3message fill in
		pack_data.type = MESSAGE;
		strcpy(pack_data.source , me.alias);
		//memcpy(pack_data.data, message, LINEBUF);
		snprintf(pack_data.data, LINEBUF, "%s %s",command_type, message);
		pack_data.size = strlen(pack_data.data);
		
		sendtoall(&pack_data, &me);
		
	}

	else {
		fprintf(stderr, "Invalid command.\n");
	}


	//erase lab3message pack_data information for a new command
	memset(&pack_data, 0, sizeof(struct lab3message));
	memset(&command[0], 0, sizeof(command));
	endcommand = NULL; 
    }

    return 0;
}
 

/* 	Need to modify login so it takes in user input server ip and server port 
	to connect to server, right now it is hard-coded at the top of the file
	Also need to pass in password to server to authenticate

	Need to implement joinsession (need to pass in command and session id to server),
	leave session (need to pass in command), createsession (need to pass in command and session id to server),
	list (need to pass in command) */

void login(struct lab3message *pack__data, struct USER *me, char* server_IP, char* server__port) {
    
    struct lab3message pack_data;
    pack_data.type = pack__data->type;
    pack_data.size = pack__data->size;
    strcpy(pack_data.source, pack__data->source);
    strcpy(pack_data.data, pack__data->data);

    int recvc, recvd, server_port;
    server_port = atoi(server__port);
    /*
    //printf("server port is %d\n", server_port);
    if(isconnected) {
        fprintf(stderr, "You are already connected to server at %s:%d\n", server_IP, server_port);
        return;
    }*/

	//connecting with the server and sending the login packet
	sockfd = connect_with_server(server_IP, server_port);
	if(sockfd >= 0) {
		isconnected = 1;
		me->sockfd = sockfd;
		printf("packet type is %d\n", pack_data.type);
		recvc = send(sockfd, (void *)&pack_data, sizeof(struct lab3message), 0);
		if (recvc < 0) {
			fprintf(stderr, "[Client]: Login Error...\n");
		}
	}

	else {
        fprintf(stderr, "[Client]: Connection rejected...\n");
    	}
	 
	//getting acknowledgement for login packet
	struct lab3message packet;
	recvd = recv(sockfd, (void *)&packet, sizeof(struct lab3message), 0);
		if(!recvd) {
		    fprintf(stderr, "[Client]: Login connection lost from server...\n");
		    isconnected = 0;
		    close(sockfd);
		    return;
		}

	if(recvd > 0) {

			if (packet.type == LO_ACK) {
				struct THREADINFO threadinfo;
				pthread_create(&threadinfo.thread_ID, NULL, receiver, (void *)&threadinfo);
				printf("[Client]: Login for client [%s] succeeded\n", packet.source);
			}
			else if (packet.type == LO_NAK){
				printf("[Client]: Login for client [%s] failed: %s\n", packet.source, packet.data);
			}

		}	    		
	
	//cleaning out the packet from server
	memset(&packet, 0, sizeof(struct lab3message));
    
}

void logout(struct lab3message *pack__data, struct USER *me) {
    int sent;

    struct lab3message pack_data;
    pack_data.type = pack__data->type;
    pack_data.size = pack__data->size;
    strcpy(pack_data.source, pack__data->source);
    strcpy(pack_data.data, pack__data->data);

    if(!isconnected) {
		fprintf(stderr, "[Client]: You are not connected to the server.\n");
		return;
    	}

    
    /* send request to close this connetion */
    sent = send(sockfd, (void *)&pack_data, sizeof(struct lab3message), 0);
}

void leave_session (struct lab3message *pack__data, struct USER *me) {
    int sent;

    struct lab3message pack_data;
    pack_data.type = pack__data->type;
    pack_data.size = pack__data->size;
    strcpy(pack_data.source, pack__data->source);
    strcpy(pack_data.data, pack__data->data);


   if(!isconnected) {
		fprintf(stderr, "[Client]: You are not connected to the server.\n");
		return;
    	}
    /* send request to leave session */
    insession = 0;
    sent = send(sockfd, (void *)&pack_data, sizeof(struct lab3message), 0);
}

 
int connect_with_server(char* server_IP,int server_port) {
    int newfd, err_ret;
    struct sockaddr_in serv_addr;
    struct hostent *to;
 
 	printf("server_IP is %s, server port is %d\n", server_IP, server_port);
    /* generate address */
    if((to = gethostbyname(server_IP))==NULL) {
        err_ret = errno;
        fprintf(stderr, "gethostbyname() error...\n");
        return err_ret;
    }
 
    /* open a socket */
    if((newfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        err_ret = errno;
        fprintf(stderr, "socket() error...\n");
        return err_ret;
    }
 
    /* set initial values */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr = *((struct in_addr *)to->h_addr);
    memset(&(serv_addr.sin_zero), 0, 8);
 
    /* try to connect with server */
    if(connect(newfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
        err_ret = errno;
        fprintf(stderr, "connect() error...\n");
        return err_ret;
    }
    else {
        printf("Connected to server at %s:%d\n", server_IP, server_port);
        return newfd;
    }
}

void create_session(struct lab3message *pack__data, struct USER *me){
	int recvc, recvd;

	struct lab3message pack_data;
    pack_data.type = pack__data->type;
    pack_data.size = pack__data->size;
    strcpy(pack_data.source, pack__data->source);
    strcpy(pack_data.data, pack__data->data);

	if(!isconnected) {
		fprintf(stderr, "[Client]: You are not connected to the server.\n");
		return;
    	}

	recvc = send(sockfd, (void *)&pack_data, sizeof(struct lab3message), 0);
	if (recvc < 0) {
		fprintf(stderr, "[Client]: New Session Creation Error...\n");
	}

	//getting acknowledgement for joinsession packet
	struct lab3message packet;
	//printf("Before getting acknowledgement in createsession function\n");
/*	recvd = recv(sockfd, (void *)&packet, sizeof(struct lab3message), 0);
  
	printf("After getting acknowledgement in createsession function\n");
	if(!recvd) {
	    fprintf(stderr, "[Client]: New session connection lost from server...\n");
	    isconnected = 0;
	    close(sockfd);
	    return;
	}

	if(recvd > 0) {
		if (packet.type == NS_ACK) {
			insession = 1;
			printf("[Client]: Creating new session ID [%s] for client [%s] succeeded\n", packet.data, packet.source);
		}
		else if (packet.type == NS_NAK){
			char* ns_nak_data, * endcommand; 
			strcpy(ns_nak_data, packet.data);
			char* session_ID = strtok_r(ns_nak_data, ",", &endcommand);
			char* reason = strtok_r(NULL, "\n", &endcommand);
			fprintf(stderr, "[Client]: Creating new session ID [%s] for client [%s] failed. Error: %s\n", session_ID, packet.source, reason);

		}	
		else  
		fprintf(stderr, "[Client]: Creating new session ID [%s] for client [%s] failed.\n", packet.data, packet.source);
	    		
	}
	//cleaning out the packet from server */
	memset(&packet, 0, sizeof(struct lab3message));
}

void join_session(struct lab3message *pack__data, struct USER *me){
	int recvc, recvd;

	struct lab3message pack_data;
    pack_data.type = pack__data->type;
    pack_data.size = pack__data->size;
    strcpy(pack_data.source, pack__data->source);
    strcpy(pack_data.data, pack__data->data);

	if(!isconnected) {
		fprintf(stderr, "[Client]: You are not connected to the server.\n");
		return;
    	}
	recvc = send(sockfd, (void *)&pack_data, sizeof(struct lab3message), 0);
	if (recvc < 0) {
		fprintf(stderr, "[Client]: Join Session Error...\n");
	}
	
	//getting acknowledgement for joinsession packet
	struct lab3message packet;
	//printf("Before receiving in join function\n");
 /*	recvd = recv(sockfd, (void *)&packet, sizeof(struct lab3message), 0);

	printf("After receiving in join function\n");
	if(!recvd) {
	    fprintf(stderr, "[Client]: Login connection lost from server...\n");
	    isconnected = 0;
	    close(sockfd);
	    return;
	}

	if(recvd > 0) {
		printf("Hi we're here in the join function acknowledgement\n");
		if (packet.type == JN_ACK ) {
			insession = 1;
			printf("[Client]: Joining session ID [%s] for client [%s] succeeded\n", packet.data, packet.source);
		}
		else if (packet.type == JN_NAK){
			char jn_nak_data[MAX_DATA], * endcommand; 
			strcpy(jn_nak_data, packet.data);
			char* session_ID = strtok_r(jn_nak_data, ",", &endcommand);
			char* reason = strtok_r(NULL, "\n", &endcommand);
			fprintf(stderr, "[Client]: Joining session ID [%s] for client [%s] failed. Error: %s\n", session_ID, packet.source, reason);

		}	
		else  
			fprintf(stderr, "[Client]: Joining session ID [%s] for client [%s] failed.\n", packet.data, packet.source);

	} */
	//cleaning out the packet from server 
	memset(&packet, 0, sizeof(struct lab3message));
}

void list(struct lab3message *pack__data, struct USER *me){
	int recvc, recvd;

	struct lab3message pack_data;
    pack_data.type = pack__data->type;
    pack_data.size = pack__data->size;
    strcpy(pack_data.source, pack__data->source);
    strcpy(pack_data.data, pack__data->data);

	if(!isconnected) {
		fprintf(stderr, "[Client]: You are not connected to the server.\n");
		return;
    	}
	recvc = send(sockfd, (void *)&pack_data, sizeof(struct lab3message), 0);
	if (recvc < 0) {
		fprintf(stderr, "[Client]: Query Error...\n");
	}

	//getting acknowledgement for list packet
	struct lab3message packet;
/*	recvd = recv(sockfd, (void *)&packet, sizeof(struct lab3message), 0);
	 
	if(!recvd) {
	    fprintf(stderr, "[Client]: Query connection lost from server...\n");
	    isconnected = 0;
	    close(sockfd);
	    return;
	}

	if(recvd > 0) {
		char listing[MAX_DATA], * endcommand, *client; 
		strcpy(listing, packet.data);
		int count = 0;

		//2 string arrays: 1 for client, 2 for ids
		char client_array [MAX_CLIENTS][MAX_NAME];
		char id_array [MAX_CLIENTS][MAX_NAME];

		//loop until client becomes NULL
		while (client = strtok_r(listing, ",", &endcommand)){
			if ((client == NULL) && (count == 0)){
				printf("There are no avaiable sessions.\n");
				break;
			}

			if (count%2 == 0) { //even numbers are clients					
				strcpy(client_array [count], client); 
			}

			else if (count%2 == 1) { //odd numbers are session ids
				strcpy(id_array [count-1], client);
			}
			count++;
			strcpy(listing , endcommand); //leftover listing
		}

		//print the listing out
		printf("The following are the available sessions and the respective users in each session.\n");
		int i, j;
		for (i = 0; i < MAX_CLIENTS ; i++){
			if (!strcmp(client_array[i], ""))
				break;
			if (id_array[i][0] != '$') { //you can print it out
				printf("Session %d: %s\n", i+1, id_array[i]);
				printf("%s\n", client_array[i]); //corresponding client

				//go through the loop for matching sessions
				for (j = 0; j < MAX_CLIENTS ; j++){
					if (!strcmp( id_array[i], id_array[j] ) && i!=j){
						printf("%s\n", client_array[j]);
						id_array[j][0] == '$';
					}
				}
				printf("\n");
			}
		}

		printf("Query Ended.\n");			    		
	} */
	//cleaning out the packet from server
	memset(&packet, 0, sizeof(struct lab3message));
}
 
void *receiver(void *param) {
    int recvd;
    struct lab3message packet;

    printf("Waiting here [%d]...\n", sockfd);

    while(isconnected) {
        //printf("In receiver loop\n");
        recvd = recv(sockfd, (void *)&packet, sizeof(struct lab3message), 0);
        if(!recvd) {
            fprintf(stderr, "[Receiver]: Connection lost from server...\n");
            isconnected = 0;
            close(sockfd);
            break;
        }
        if(recvd > 0) {
			//printf("Receiver packet:[%s]: %s\n", packet.source, packet.data);

						
			if (packet.type == QU_ACK) {
				char listing[MAX_DATA], * endcommand, *client; 
				strcpy(listing, packet.data);
				int count = 0;
				int client_count = 0, id_count = 0;

				//2 string arrays: 1 for client, 2 for ids
				char client_array [MAX_CLIENTS][MAX_NAME];
				char id_array [MAX_CLIENTS][MAX_NAME];
				memset(client_array, 0, sizeof(client_array[0][0]) * MAX_CLIENTS * MAX_NAME);
				memset(id_array, 0, sizeof(id_array[0][0]) * MAX_CLIENTS * MAX_NAME);

				//loop until client becomes NULL
				while (client = strtok_r(listing, ",", &endcommand)){
					if ((client == NULL) && (count == 0)){
						printf("[Receiver]: There are no avaiable sessions.\n");
						break;
					}

					if (count%2 == 0) { //even numbers are clients					
						strcpy(client_array [client_count], client);
						client_count++;
						//printf("In loop: %s\n", client_array [count]); 
					}

					else if (count%2 == 1) { //odd numbers are session ids
						strcpy(id_array [id_count], client);
						id_count++;
						//printf("In loop: %s\n", id_array [count-1]); 

					}
					count++;
					strcpy(listing , endcommand); //leftover listing
				}

				//print the listing out
				printf("[Receiver]: The following are the available sessions and the respective users in each session.\n");
				int i, j;
				int session_count = 1;
				for (i = 0; i < MAX_CLIENTS ; i++){
					
					if (!strcmp(client_array[i], ""))
						
						break;
					//printf("Middle here\n");
					if (strcmp(id_array[i], "$")) { // if it's not dollar you can print it out
						
						printf("Session %d: %s\n", session_count, id_array[i]);
						printf("%s\n", client_array[i]); //corresponding client
						
					
					//go through the loop for matching sessions
					for (j = 0; j < MAX_CLIENTS ; j++){
						if (!strcmp( id_array[i], id_array[j] ) && i!=j){
							printf("%s\n", client_array[j]);
							strcpy(id_array[j] , "$");
						}
					}
					strcpy(id_array[i] , "$");
					printf("\n");
					session_count++;
					}
				}
				
				printf("Query Ended.\n");
				
			}

			if (packet.type == JN_ACK || packet.type == JN_NAK ) {
				//printf("Hi we're here in the join acknowledgement\n");
				if (packet.type == JN_ACK ) {
					insession = 1;
					printf("[Receiver]: Joining session ID [%s] for client [%s] succeeded\n", packet.data, packet.source);
				}
				else if (packet.type == JN_NAK){
					char jn_nak_data[MAX_DATA], * endcommand; 
					strcpy(jn_nak_data, packet.data);
					char* session_ID = strtok_r(jn_nak_data, ",", &endcommand);
					char* reason = strtok_r(NULL, "\n", &endcommand);
					fprintf(stderr, "[Receiver]: Joining session ID [%s] for client [%s] failed.Error: %s\n", session_ID, packet.source, reason);

				}	
				else  
					fprintf(stderr, "[Receiver]: Joining session ID [%s] for client [%s] failed.\n", packet.data, packet.source);
				//printf("End of receiver join brackets\n");
				
			}

			if (packet.type == NS_ACK || packet.type == NS_NAK) {
						if (packet.type == NS_ACK) {
					insession = 1;
					printf("[Receiver]: Creating new session ID [%s] for client [%s] succeeded\n", packet.data, packet.source);
				}
				else if (packet.type == NS_NAK){
						char* ns_nak_data, * endcommand; 
						strcpy(ns_nak_data, packet.data);
						char* session_ID = strtok_r(ns_nak_data, ",", &endcommand);
						char* reason = strtok_r(NULL, "\n", &endcommand);
						fprintf(stderr, "[Receiver]: Creating new session ID [%s] for client [%s] failed. Error: %s\n", session_ID, packet.source, reason);

					}	
					else  
						fprintf(stderr, "[Receiver]: Creating new session ID [%s] for client [%s] failed.\n", packet.data, packet.source);
				
			}

			if (packet.type == LO_ACK || packet.type == LO_NAK) {

					if (packet.type == LO_ACK) {
						//struct THREADINFO threadinfo;
			        		//pthread_create(&threadinfo.thread_ID, NULL, receiver, (void *)&threadinfo);
						printf("[Receiver]: Login for client [%s] succeeded\n", packet.source);
					}
					else if (packet.type == LO_NAK){
						printf("[Receiver]: Login for client [%s] failed: %s\n", packet.source, packet.data);
					}
				
        		}	

			if (packet.type == MESSAGE) {
				char message[MAX_DATA + MAX_NAME];
				strcat(message, "[");
				strcat(message, packet.source);
				strcat(message, "]: ");
				strcat(message, packet.data);
				strcat(message, "\n");
				puts(message);
				//printf("[%s]: %s\n", packet.source, packet.data);
				strcpy(message, "");
			}


        memset(&packet, 0, sizeof(struct lab3message));
    	}
	//printf("End of receiver recvd brackets\n");
	}
	//printf("End of receiver isconnected brackets\n");
    return;
}
 
void sendtoall(struct lab3message *pack__data, struct USER *me) {
	int recvc;

	struct lab3message pack_data;
    	pack_data.type = pack__data->type;
    	pack_data.size = pack__data->size;
    	strcpy(pack_data.source, pack__data->source);
    	strcpy(pack_data.data, pack__data->data);

	if(!isconnected) {
		fprintf(stderr, "[Client]: You are not connected to the server.\n");
		return;
    	}
	//printf("What I'm sending is: %s\n", pack_data.data);
	recvc = send(sockfd, (void *)&pack_data, sizeof(struct lab3message), 0);
	if (recvc < 0) {
		fprintf(stderr, "Message sending failed.\n");
	}
}

