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
 
#define IP "127.0.0.1"
#define BACKLOG 10
#define MAX_CLIENTS 10
#define MAX_SESSIONS 10
 
#define LINEBUF 2048 
#define MAX_DATA 1024
#define MAX_NAME 32
#define OPTLEN 16
#define MAX_PASSWORD 50
#define MAX_SESSION_ID 32

#define LOGIN 100
#define LO_ACK 101
#define LO_NACK 102
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

char passwords[MAX_CLIENTS][MAX_PASSWORD];
char users[MAX_CLIENTS][MAX_NAME];

struct lab3message {
    unsigned int type; // instruction
	unsigned int size; // length of data
    unsigned char source[MAX_NAME]; // client's alias
    char data[MAX_DATA]; // payload
};
 
struct THREADINFO {
    pthread_t thread_ID; // thread's pointer
    int sockfd; // socket file descriptor
    char alias[MAX_NAME]; // client's alias
    char current_session[MAX_SESSION_ID];
};
 
struct LLNODE {
    struct THREADINFO threadinfo;
    struct LLNODE *next;
};
 
struct LLIST {
    struct LLNODE *head, *tail;
    int size;
    char session_ID[MAX_SESSION_ID];
		/*we can make an array of linked list and
			each list list represents a session containing
			the corresponding clients*/
};
 
int compare(struct THREADINFO *a, struct THREADINFO *b) {
    return a->sockfd - b->sockfd;
}
 
void list_init(struct LLIST *ll) {
    ll->head = ll->tail = NULL;
    ll->size = 0;
}
 
int list_insert(struct LLIST *ll, struct THREADINFO *thr_info) {
    if(ll->size == MAX_CLIENTS) return -1;
    if(ll->head == NULL) {
        ll->head = (struct LLNODE *)malloc(sizeof(struct LLNODE));
        ll->head->threadinfo = *thr_info;
        ll->head->next = NULL;
        ll->tail = ll->head;
    }
    else {
        ll->tail->next = (struct LLNODE *)malloc(sizeof(struct LLNODE));
        ll->tail->next->threadinfo = *thr_info;
        ll->tail->next->next = NULL;
        ll->tail = ll->tail->next;
    }
    ll->size++;
    return 0;
}
 
int list_delete(struct LLIST *ll, struct THREADINFO *thr_info) {
    struct LLNODE *curr, *temp;
    if(ll->head == NULL) return -1;
    if(compare(thr_info, &ll->head->threadinfo) == 0) {
        temp = ll->head;
        ll->head = ll->head->next;
        if(ll->head == NULL) ll->tail = ll->head;
        free(temp);
        ll->size--;
        return 0;
    }
    for(curr = ll->head; curr->next != NULL; curr = curr->next) {
        if(compare(thr_info, &curr->next->threadinfo) == 0) {
            temp = curr->next;
            if(temp == ll->tail) ll->tail = curr;
            curr->next = curr->next->next;
            free(temp);
            ll->size--;
            return 0;
        }
    }
    return -1;
}
 
void list_dump(struct LLIST *ll) {
    struct LLNODE *curr;
    struct THREADINFO *thr_info;
    printf("Connection count: %d\n", ll->size);
    for(curr = ll->head; curr != NULL; curr = curr->next) {
        thr_info = &curr->threadinfo;
        printf("[%d] %s %s\n", thr_info->sockfd, thr_info->alias, thr_info->current_session);
    }
}
 
int sockfd, newfd;
int num_sessions;
struct THREADINFO thread_info[MAX_CLIENTS];
struct LLIST client_list;
struct LLIST sessions[MAX_SESSIONS];
 
void *client_handler(void *fd);

int main(int argc, char **argv) {

    strcpy(users[0], "Alice");
    strcpy(users[1], "Bree");
    strcpy(users[2], "Cathy");
    strcpy(users[3], "Dylan");
    strcpy(users[4], "Ethan");
    strcpy(users[5], "Francis");
    strcpy(users[6], "Greg");
    strcpy(users[7], "Helen");
    strcpy(users[8], "Ian");
    strcpy(users[9], "Jenny");

    strcpy(passwords[0], "aaa");
    strcpy(passwords[1], "bbb");
    strcpy(passwords[2], "ccc");
    strcpy(passwords[3], "ddd");
    strcpy(passwords[4], "password4E");
    strcpy(passwords[5], "password5F");
    strcpy(passwords[6], "password6G");
    strcpy(passwords[7], "password7H");
    strcpy(passwords[8], "password8I");
    strcpy(passwords[9], "password9J");

    int err_ret, sin_size;
    struct sockaddr_in serv_addr, client_addr;
 
    /* initialize linked list */
    list_init(&client_list);
 
    int i;
    for(i = 0; i < MAX_SESSIONS; i++)
    {
        list_init(&sessions[i]);
        memset(sessions[i].session_ID, 0, sizeof(sessions[i].session_ID));
    }

    /* open a socket */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        err_ret = errno;
        fprintf(stderr, "[ERROR] socket() failed...\n");
        return err_ret;
    }
 
    /* set initial values */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = inet_addr(IP);
    memset(&(serv_addr.sin_zero), 0, 8);
 
    /* bind address with socket */
    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
        err_ret = errno;
        fprintf(stderr, "[ERROR] bind() failed...\n");
        return err_ret;
    }
 
    /* start listening for connection */
    if(listen(sockfd, BACKLOG) == -1) {
        err_ret = errno;
        fprintf(stderr, "[ERROR] listen() failed...\n");
        return err_ret;
    }
 
    /* initiate interrupt handler for IO controlling */
    printf("[SERVER] Starting server interface...\n");
 
    /* keep accepting connections */
    printf("[SERVER] Starting socket listener...\n");
    while(1) {
        sin_size = sizeof(struct sockaddr_in);
        if((newfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&sin_size)) == -1) {
            err_ret = errno;
            fprintf(stderr, "[ERROR] accept() failed...\n");
            return err_ret;
        }
        else {
            if(client_list.size == MAX_CLIENTS) {
                fprintf(stderr, "[ERROR] Connection full, request rejected...\n");
                continue;
            }
            printf("[SERVER] Connection requested received...\n");
            struct THREADINFO threadinfo;
            threadinfo.sockfd = newfd;
            pthread_create(&threadinfo.thread_ID, NULL, client_handler, (void *)&threadinfo);
        }
    }
    return 0;
}
 
 void *client_handler(void *fd) {
    struct THREADINFO threadinfo = *(struct THREADINFO *)fd;
    struct lab3message packet;

    int bytes, sent;
    while(1) {
        bytes = recv(threadinfo.sockfd, (void *)&packet, sizeof(struct lab3message), 0);
        if(!bytes) {
            fprintf(stderr, "Connection lost from [%d] %s...\n", threadinfo.sockfd, threadinfo.alias);
            list_delete(&client_list, &threadinfo);
            break;
        }
        printf("[DEBUG] Packet Type Is %d\n", packet.type);

		if(packet.type == LOGIN){
			/*need to implement password authentication
				return to client LO_ACK for success
				and LO_NAK for failure
			*/
        int sent;
		char request_username[MAX_NAME];
		char request_password[MAX_PASSWORD];
        char copied_data[MAX_DATA], * endcommand; 
		struct lab3message spacket;
        
        strcpy(copied_data, packet.data);
        char* r_username = strtok_r(copied_data, ",", &endcommand);
        char* r_password = strtok_r(NULL, "\n", &endcommand);
        strcpy (request_username, r_username);
        strcpy (request_password, r_password);
        strcpy(threadinfo.alias, request_username);

        int i;
        int found_user = 0;
        int found_password = 0;

			for(i = 0; i< MAX_CLIENTS; i++){
                if(found_user == 0 || found_password == 0){
                    if(!strcmp(users[i], request_username)){
                        printf("[SERVER] Found user!\n"); 
                        found_user = 1;
                        if(!strcmp(passwords[i], request_password)){
                            printf("[SERVER] Successful Authentication\n"); 
                            strcpy(threadinfo.current_session, "no_session");
                            list_insert(&client_list, &threadinfo);
                            found_password = 1;
                            spacket.type = LO_ACK;
                            strcpy(spacket.source,threadinfo.alias);
                            sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
                        }
                        else{ // wrong password
                            spacket.type = LO_NACK;
                            strcpy(spacket.source,threadinfo.alias);
                            strcpy(spacket.data, "Wrong password");
                            sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
                        }
                    }
                }
                else
                {
                    break;
                }
			}

            if(found_user == 0)
            {
                spacket.type = LO_NACK;
                strcpy(spacket.source,threadinfo.alias);
                strcpy(spacket.data, "User not found in database");
                sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
            }
            memset(&packet, 0, sizeof(struct lab3message));
            memset(&spacket, 0, sizeof(struct lab3message));
		}
		else if(packet.type == JOIN){

            int sent, i;
            int found_session = 0;
            struct lab3message spacket;
            struct LLNODE *curr;

            char alias[MAX_NAME];
            char requested_session_ID[MAX_SESSION_ID];

            strcpy(alias, packet.source);
            strcpy(requested_session_ID, packet.data);

			if(!strcmp(threadinfo.current_session, "no_session")) //only users not in a session can join a new session
			{
	            for(i = 0; i < MAX_SESSIONS; i++)
	            {
	                if(found_session == 0){
                        if(!strcmp(sessions[i].session_ID, requested_session_ID)){
                            printf("[SERVER] Found Session ID\n");
                            found_session = 1;
                            strcpy(threadinfo.current_session, sessions[i].session_ID);
                            list_insert(&sessions[i], &threadinfo);
                            spacket.type = JN_ACK;
                            strcpy(spacket.source,threadinfo.alias);
                            strcat(spacket.data, requested_session_ID);
                            sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
                            //list_dump(&sessions[i]);
                            for(curr = client_list.head; curr != NULL; curr = curr->next) {
                                struct THREADINFO *thr_info = &curr->threadinfo;
                                //printf("[DEBUG] curr->threadinfo.alias is: %s, curr->threadinfo.session is: %s\n", curr->threadinfo.alias, curr->threadinfo.current_session);
                                if(!strcmp(thr_info->alias, alias)){
                                    strcpy(thr_info->current_session, requested_session_ID);
                                }

                            }
                        }
                    }
                    else{
                        break;
                    }
                }
                if(found_session == 0){
                    printf("[SERVER] Session ID Doesn't Exist!\n");
                    spacket.type = JN_NAK;
                    strcpy(spacket.source,threadinfo.alias);
                    strcat(spacket.data, requested_session_ID);
                    strcat(spacket.data, ",Session ID Doesn't Exist");
                    sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
                }
			}
			else
			{
                spacket.type = JN_NAK;
                strcpy(spacket.source,threadinfo.alias);
                strcat(spacket.data, requested_session_ID);
                strcat(spacket.data, ",Already Enrolled In A Session");
                sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
			}
            memset(&packet, 0, sizeof(struct lab3message));
            memset(&spacket, 0, sizeof(struct lab3message));

		}
		else if(packet.type == LEAVE_SESS){
	
			int i,j;
            int found_session = 0;
            struct LLNODE *curr;
            char alias[MAX_NAME];

            strcpy(alias, packet.source);

            if(strcmp(threadinfo.current_session, "no_session")){//only clients with session ID is allowed to leave
                for(i = 0; i < MAX_SESSIONS; i++){
                    if(found_session == 0){ // we haven't found the session that the client is in
                        if(sessions[i].size > 0){ // and the session is not empty
                            if(!strcmp(sessions[i].session_ID, threadinfo.current_session)){ 
                                found_session = 1;
                                list_delete(&sessions[i], &threadinfo);
                                strcpy(threadinfo.current_session, "no_session");
                                printf("[SERVER] Removed Client From Session %s\n", sessions[i].session_ID);
                                //list_dump(&sessions[i]);
                                for(curr = client_list.head; curr != NULL; curr = curr->next){
                                    struct THREADINFO *thr_info = &curr->threadinfo;
                                    if(!strcmp(thr_info->alias, alias)){
                                        strcpy(thr_info->current_session, "no_session");
                                    }
                                }
                            }
                        }
                    }
                    else{ // we already left the session
                        break;
                    }                
                }
            }
            else{ // Client not enrolled in any session
                printf("[SERVER] Client Not Enrolled In Any Session\n");
            }

            memset(&packet, 0, sizeof(struct lab3message));

            // destroy empty sessions
            for(j=0; j < MAX_SESSIONS; j++){
                if(sessions[j].size == 0 && sessions[j].session_ID[0] != '\0'){
                    memset(sessions[j].session_ID, 0, sizeof(sessions[j].session_ID));
                    num_sessions--;
                    printf("[SERVER] Destroyed Empty Sessions\n");

                }
            }
		}
		else if(packet.type == NEW_SESS){
			/* Create a new conference session and join it
			*/
            // REMEMBER TO MAKE SURE THAT USERS ALREADY IN A SESSION CAN"T JOIN A NEW SESSION
            int sent,i;
            int created_session = 0;
            struct lab3message spacket;
            struct LLNODE *curr;

            char requested_session_ID[MAX_SESSION_ID];
            char alias[MAX_NAME];

            strcpy(alias, packet.source);
            strcpy(requested_session_ID, packet.data); // parse the packet

            printf("current session is: %s\n", threadinfo.current_session);
			if(!strcmp(threadinfo.current_session, "no_session")){
                if(num_sessions < MAX_SESSIONS){ //make sure we have enough space to create sessiosn
		          	for(i = 0; i < MAX_SESSIONS; i++){
                        if(created_session == 0){
                            if(sessions[i].size == 0 && sessions[i].session_ID[0] == '\0'){
                                printf("[SERVER] Found Slot For New Session\n");
                                strcpy(sessions[i].session_ID, requested_session_ID);
                                strcpy(threadinfo.current_session, sessions[i].session_ID);
                                list_insert(&sessions[i], &threadinfo);
                                spacket.type = NS_ACK;
                                strcpy(spacket.source,threadinfo.alias);
                                strcpy(spacket.data, sessions[i].session_ID);
                                sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
                                created_session = 1;
                                num_sessions++;
                                //list_dump(&sessions[i]);

                                for(curr = client_list.head; curr != NULL; curr = curr->next) {
                                    struct THREADINFO *thr_info = &curr->threadinfo;
                                    //printf("[DEBUG] curr->threadinfo.alias is: %s, curr->threadinfo.session is: %s\n", curr->threadinfo.alias, curr->threadinfo.current_session);
                                    if(!strcmp(thr_info->alias, alias)){
                                        strcpy(thr_info->current_session, requested_session_ID);
                                    }

                                }
                            }
                        }
                        else{
                            // already created a new session
                            break;
                        }
                    }
                }
                else{
					spacket.type = NS_NAK;
                    strcpy(spacket.source,threadinfo.alias);
					strcat(spacket.data, requested_session_ID);
					strcat(spacket.data, ",Exceeded Total Session Allowance");
					sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
				}
			}
			else
			{
    			spacket.type = NS_NAK;
                strcpy(spacket.source,threadinfo.alias);
    			strcat(spacket.data, requested_session_ID);
    			strcat(spacket.data, ",Already Enrolled In A Session");
    			sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
			}
            memset(&packet, 0, sizeof(struct lab3message));
            memset(&spacket, 0, sizeof(struct lab3message));

		}
		else if(packet.type == QUERY){
			/*list all the sessions that users are in
			*/
            int sent;
            int count = 1;
            struct LLNODE *curr;
            struct lab3message spacket;

            for(curr = client_list.head; curr != NULL; curr = curr->next) {
                //printf("[DEBUG] curr->threadinfo.alias is: %s, curr->threadinfo.session is: %s\n", curr->threadinfo.alias, curr->threadinfo.current_session);
                if(count < client_list.size){
                    strcat(spacket.data, curr->threadinfo.alias);
                    strcat(spacket.data, ",");
                    strcat(spacket.data, curr->threadinfo.current_session);
                    count++;
                    strcat(spacket.data, ",");
                }
                else{
                    strcat(spacket.data, curr->threadinfo.alias);
                    strcat(spacket.data, ",");
                    strcat(spacket.data, curr->threadinfo.current_session);
                    count++;
                }
            }
            spacket.type = QU_ACK;
            strcpy(spacket.source,threadinfo.alias);
            sent = send(threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
            memset(&packet, 0, sizeof(struct lab3message));
            memset(&spacket, 0, sizeof(struct lab3message));


		}
        else if (packet.type == MESSAGE){
            int sent,i;
            int session_found = 0;
            char message[MAX_DATA];
            struct LLNODE *curr;
            strcpy(message, packet.data);

            for(i = 0; i < MAX_SESSIONS; i++){
                if(session_found == 0){
                    if(!strcmp(sessions[i].session_ID, threadinfo.current_session)){
                        // found correct session to broadcast message
                        session_found = 1;
                        for(curr = sessions[i].head; curr != NULL; curr = curr->next) {
                            struct lab3message spacket;
                            memset(&spacket, 0, sizeof(struct lab3message));
                            if(!compare(&curr->threadinfo, &threadinfo)) continue;
                            spacket.type = MESSAGE;
                            strcpy(spacket.source, threadinfo.alias);
                            strcpy(spacket.data, message);
                            sent = send(curr->threadinfo.sockfd, (void *)&spacket, sizeof(struct lab3message), 0);
                            memset(&spacket, 0, sizeof(struct lab3message));

                        }
                    }
                }
                else
                {
                    break;
                }   
            }
            memset(&packet, 0, sizeof(struct lab3message));

        }
        else if (packet.type == EXIT){
            printf("[SERVER] Exiting from server\n");
            //printf("[%d] %s has disconnected...\n", threadinfo.sockfd, threadinfo.alias);
			int i;
            //printf()
            for(i = 0; i < MAX_SESSIONS; i++){
                if(sessions[i].size > 0 ){
                    list_delete(&sessions[i], &threadinfo);
                }
            }
            list_delete(&client_list, &threadinfo);
            memset(&packet, 0, sizeof(struct lab3message));

        }
        else if(packet.type == QUIT){
            int i;

            printf("[SERVER] Terminating program\n");

            for(i = 0; i < MAX_SESSIONS; i++){
                if(sessions[i].size > 0 ){
                    list_delete(&sessions[i], &threadinfo);
                }
            }
            list_delete(&client_list, &threadinfo);
            memset(&packet, 0, sizeof(struct lab3message));
            close(sockfd);
            exit(0);
        }
        else {
            fprintf(stderr, "[SERVER] Garbage data from [%d] %s...\n", threadinfo.sockfd, threadinfo.alias);
        }
    }
 
    /* clean up */
    close(threadinfo.sockfd);
 
    return NULL;
}
