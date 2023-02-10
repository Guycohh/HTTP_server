#include <stdbool.h>
#include "threadpool.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "stdlib.h"
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
//------------------------------------------------------------functions------------------------------------------------------------
char *get_mime_type(char *name);
bool isnumber (char* str);
int responseFunction(void * arg);// get client sd and handle it.
void bad_errors(char *response, char *path, int * sd);
void check_path(char* response, char *path, int *sd);
void ok_response(unsigned char * file_content,char* response, char *path, int file_len,  char *modified ,int *sd);
void dir_response(char* response, char *path0,char **dir_content, int dirs_amount,  char *modified ,int *sd);
bool have_permissions(char *path);//return true if there are permissions.
void fileContent(char* response, char *path,int *sd);
void error_500(char * response, char *path,int *sd);
//------------------------------------------------------------functions------------------------------------------------------------
//------------------------------------------------------------defines------------------------------------------------------------
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define USAGE_ERR_WRONG_COMMAND "Usage: server <port> <pool-size> <max-number-of-request>\n"
#define BAD_REQUEST_400 "HTTP/1.0 400 Bad Request\r\nServer: webserver/1.0\r\nDate: "
#define BAD_REQUEST_501 "HTTP/1.0 501 Not supported\r\nServer: webserver/1.0\r\nDate: "
#define BAD_REQUEST_500 "HTTP/1.0 500 Internal Server Error\r\nServer: webserver/1.0\r\nDate: "
#define BAD_REQUEST_404 "HTTP/1.0 404 Not Found\r\nServer: webserver/1.0\r\nDate: "
#define BAD_REQUEST_403 "HTTP/1.0 403 Forbidden\r\nServer: webserver/1.0\r\nDate: "
#define BAD_302 "HTTP/1.0 302 Found\r\nServer: webserver/1.0\r\nDate: "
#define OK_200 "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: "
#define CONTENT_TYPE "\r\nContent-Type: text/html\r\nContent-Length: "
#define CONNECTION_CLOSE "\r\nConnection: close\r\n\r\n"
#define BODY "</TITLE></HEAD>\r\n<BODY><H4>"
#define H4 "</H4>\r\n"
#define TITLE "<HTML><HEAD><TITLE>"
#define END_BODY "\r\n</BODY></HTML>\r\n"
#define LOCATION "\r\nLocation: "
#define DIR_START "<HTML>\r\n<HEAD><TITLE>Index of "
#define DIR_TITLE "</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of "
#define DIR_H4 "<H4>Index of "
#define TABLE_DIR "\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n\r\n"
#define END_DIR "\r\n</table>\r\n\r\n<HR>\r\n\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n\r\n</BODY></HTML>\r\n"//for each entity:
#define ENTITY0 "<tr>\r\n<td><A HREF=\""
#define ENTITY1 "\">"
#define ENTITY2 "</A></td><td>"
#define ENTITY3 "</td>\r\n<td>"
#define ENTITY4 "</td>\r\n</tr>\r\n"
//------------------------------------------------------------defines------------------------------------------------------------

int main (int argc , char** argv) {
    /*first, I want to be sure that the command is legal.*/
    if(argc!=4 || !isnumber(argv[1]) || !isnumber(argv[2]) || !isnumber(argv[3])){
        printf(USAGE_ERR_WRONG_COMMAND);
        exit(EXIT_FAILURE);
    }
    /*I want to know if the port is legal, if it is smaller than 65535.
     *I want to know if the pool amount is legal, if it is smaller than 0.
     *I want to know if the max number of request is legal, if it is smaller than 0*/
    int port= (int)atoi(argv[1]);
    int pool_size=(int)atoi(argv[2]);
    int max_number_of_request=(int)atoi(argv[3]);
    if(port > 65535 || port <0 || pool_size<0 || max_number_of_request<0){
        printf(USAGE_ERR_WRONG_COMMAND);
        exit(EXIT_FAILURE);
    }
    //--------------------------init server socket------------------------
    threadpool * thread_pool=create_threadpool(pool_size);
    if(thread_pool==NULL){
        printf(USAGE_ERR_WRONG_COMMAND);
        exit(EXIT_FAILURE);
    }
    int welcome_socket;  //create an endpoint for communication
    if((welcome_socket= socket( PF_INET, SOCK_STREAM , 0) )< 0){ //checking success
        perror("ERROR with create your socket fd");
        exit(1);
    }
    struct sockaddr_in peeraddr;
    peeraddr.sin_family=AF_INET;
    peeraddr.sin_addr.s_addr=htonl(INADDR_ANY);
    peeraddr.sin_port= htons(port);
    if(bind(welcome_socket, (struct sockaddr *)&peeraddr, sizeof(peeraddr))<0){//bind the socket to the port we want.
        destroy_threadpool(thread_pool);
        close(welcome_socket);
        perror("\nERROR with bind\n");
        exit(1);
    }
    if(listen(welcome_socket,5)<0){//good number for a queue is 5...
        destroy_threadpool(thread_pool);
        close(welcome_socket);
        perror("\nERROR with listen\n");
        exit(1);
    }

    int connection_socket[max_number_of_request];// with this socket I will talk with the client.
    for(int i=0; i<max_number_of_request; i++){
//        printf("\n Waiting for connections:\n");
        if((*(connection_socket+i)=accept(welcome_socket,(struct sockaddr *)NULL,NULL))<0){
            close(welcome_socket);
            perror("\nERROR with accept\n");
            exit(-1);
        }
//        printf("\nConnected!\n");


        dispatch(thread_pool,responseFunction, connection_socket+i );
    }

    destroy_threadpool(thread_pool);
    close(welcome_socket);

    return 0;
}

char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}
//-------------------check_if_is_number----------------
// checks if string is a number.
bool isnumber (char* str){
    if(str==NULL)
        return false;
    for(int i = 0 ; i < (int)strlen(str) ; i++)//for each char, check if it is a digit or not.
        if(!isdigit(str[i]))
            return false;
    return true;
}
//-------------------check_if_is_number----------------
//------------------responseFunction-------------------
/*This function gets a client sd and handle it.
 *This function is responsible on "analyze" the response. */
int responseFunction(void * arg){
    if(!arg)
        return -1;
    signal(SIGPIPE, SIG_IGN);

    /*first, I want to check the input.
     * The request’s first line should contain a method, path, and protocol.
     * Here I have to check that there are 3 tokens and that the last one is one of the HTTP versions,
     * other checks on the method and the path will be checked later.*/
    int *sd= (int*)arg;//socket fd.
    char *response=(char*) malloc(sizeof(char ));
    response[0]=0;
    if(response==NULL){
        perror("\nERROR: malloc request failed\n");
        close(*sd);
        exit(1);
    }
    int rd=0;
    char buffer[500]="";
    char *request= (char *)malloc(sizeof(char));
    if(request==NULL){
        error_500(response, NULL, sd);
        return 0;
    }
    request[0]=0;
    memset(buffer, '0', sizeof(buffer));
    int size_read=0;
    while(true){
        memset(buffer, '0', sizeof(buffer));
        if((rd= read(*sd, buffer, sizeof(buffer)-1)) < 0) {
            error_500(response, NULL, sd);
            return 0;
        }
        if(rd==0){
            break;
        }
        size_read+=rd;
        buffer[rd]=0;
        request=(char *)realloc(request,sizeof(char )*(size_read)+1);

        if(request==NULL){
            error_500(response, NULL, sd);
            free(request);
            request=NULL;
            return 0;
        }
        strcat(request, buffer);
        request[size_read]=0;
        if(strstr(request, "\r\n")!=NULL){
            break;
        }

    }
//                    printf("\n\n\n%s\n\n\n", request);

    //Now, I want the first line.
    char method[(int)strlen(request)];
    char path[(int) strlen(request)];
    char protocol[(int)strlen(request)];
    int words_counter=0;
    char *token_enter=strtok(request, "\r\n");
    char *token= strtok(token_enter, " ");
    while (token){
        if(words_counter==0){
            strcpy(method, token);
            method[strlen(method)+1]=0;
        }
        if(words_counter==1){
            strcpy(path, token);
            path[strlen(path)+1]=0;

        }
        if(words_counter==2){
            strcpy(protocol, token);
            char *ptr=strstr(protocol,"\r\n");
            if(ptr!=NULL && (int)strlen(protocol)>8)
                protocol[8]=0;
            else
                protocol[(int)strlen(protocol)+1]=0;
        }
        words_counter++;
        token= strtok(NULL," ");
    }
    if(response==NULL){
        error_500(response, NULL, sd);
        free(request);
        request=NULL;
        return 0;
    }
    response[0]=0;
//                    printf("\n\n\n%s\n\n\n", path);

    //now I want to check the first line...
    if(words_counter!=3||( strcmp(protocol, "HTTP/1.1")!=0 && strcmp(protocol, "HTTP/1.0")!=0 )) {
        response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_400)+1);
        if(response==NULL){
            perror("\nERROR with malloc\n");
            free(response);
            response=NULL;
            free(request);
            request=NULL;
            close(*sd);
            return 0;
        }
        strcat(response, BAD_REQUEST_400);
        response[strlen(BAD_REQUEST_400)]=0;
        bad_errors(response,path, sd);
        free(request);
        request=NULL;
        return 0;
    }
    if(strcmp(method, "GET")!=0 && strcmp(method, "get")!=0) {
        response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_501)+1);
        if(response==NULL){
            perror("\nERROR with malloc\n");
            free(response);
            response=NULL;
            free(request);
            request=NULL;
            close(*sd);
            exit(1);
        }
        strcat(response, BAD_REQUEST_501);
        response[strlen(BAD_REQUEST_501)]=0;
        bad_errors(response,path, sd);
        free(request);
        request=NULL;
        return 0;

    }

    check_path(response, path, sd);

    free(request);
    request=NULL;

    return 0;
}
//------------------responseFunction-------------------
void check_path(char *response,char *path, int *sd){

    struct stat stat1;
//    stat(path, &stat1);
    /*First I want to be sure if the path is exists.
     *If true, return "404 not found". */
    if(strcmp(path,"/")!=0)
        path = path + 1;
    else
        strcpy(path,"./");
//    printf("\n\n\n%s\n\n\n", path);

    if (stat(path, &stat1) != 0) {//
        response = (char *) realloc(response, sizeof(char) * strlen(BAD_REQUEST_404) + 1);
        if (response == NULL) {
            perror("\nERROR with malloc\n");
            free(response);
            response = NULL;
            close(*sd);
            exit(1);
        }
        strcat(response, BAD_REQUEST_404);
        response[strlen(BAD_REQUEST_404)] = 0;
        bad_errors(response, path, sd);

        return;
    }
    /*If the path is a directory, but it does not end with '/'.
     * If true, return "302 Found".*/
    if(S_ISDIR(stat1.st_mode) && path[(int)strlen(path)-1]!='/'){
        response=(char*)realloc(response,sizeof(char)* strlen(BAD_302)+1);
        if(response==NULL){
            perror("\nERROR with malloc\n");
            free(response);
            response=NULL;
            close(*sd);
            exit(1);
        }
        strcat(response, BAD_302);
        response[strlen(BAD_302)]=0;
        bad_errors(response, path,sd);
        return;
    }
    /*If the path is a directory, and it ends with a '/', search for index.html
     * (a) If index.html exists, return it.
     * (b) Otherwise, return the contents of the directory.*/
    if(S_ISDIR(stat1.st_mode) && path[(int)strlen(path)-1]=='/'){
        char *path0=(char *)malloc(sizeof(char)* strlen(path)+1);
        if(path0==NULL){
            error_500(response, NULL, sd);
            free(path0);
            path0=NULL;
            exit(1);
        }
        int i=0;
        for(i=0; i< (int)strlen(path); i++){
            path0[i]=path[i];
        }
        path0[i++]=0;
        strcat(path, "index.html");
        if(have_permissions(path0)==false || have_permissions(path)==false ){//false so there is no permission
            //If I am here so there are no permissions for this directory.
            response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_403)+1);
            if(response==NULL){
                perror("\nERROR with malloc\n");
                free(response);
                response=NULL;
                close(*sd);
                exit(1);
            }
            strcat(response, BAD_REQUEST_403);
            response[strlen(BAD_REQUEST_403)]=0;
            bad_errors(response,path, sd);
            free(path0);
            return;
        }
        //strcat(path, "index.html");

        //If index.html exists, return it.
        if(stat(path, &stat1)==0){// If index.html is exists return it.
            unsigned char file_content[stat1.st_size+1];
            int rd=0;
            FILE *fp=fopen(path, "r");//open the file.
            if(fp==NULL){
                error_500(response, NULL, sd);

                return;
            }
            fseek (fp, 0, SEEK_SET);
            if((rd=fread(file_content, sizeof(char),stat1.st_size+1,fp ))==0){
                response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_403)+1);
                if(response==NULL){
                    perror("\nERROR with malloc\n");
                    free(response);
                    response=NULL;
                    close(*sd);
                    exit(1);
                }
                strcat(response, BAD_REQUEST_403);
                response[strlen(BAD_REQUEST_403)]=0;
                bad_errors(response, path,sd);
                return;
            }
            else{
                file_content[rd]=0;
                rd++;
            }
            fclose(fp);
            if(rd!= (stat1.st_size+1)){
                response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_403)+1);
                if(response==NULL){
                    perror("\nERROR with malloc\n");
                    free(response);
                    response=NULL;
                    close(*sd);
                    exit(1);
                }
                strcat(response, BAD_REQUEST_403);
                response[strlen(BAD_REQUEST_403)]=0;
                bad_errors(response, path,sd);
                return;
            }
            else{
                response=(char*)realloc(response,sizeof(char)* strlen(OK_200)+1);
                if(response==NULL){
                    perror("\nERROR with malloc\n");
                    free(response);
                    response=NULL;
                    close(*sd);
                    exit(1);
                }
                strcat(response, OK_200);
                response[strlen(OK_200)]=0;
                char modified_buffer[128];
                strftime(modified_buffer, sizeof(modified_buffer), RFC1123FMT, localtime(&stat1.st_mtime));
                ok_response(file_content,response, path, rd, modified_buffer ,sd);
                free(path0);
                return;
            }
        }
            //Otherwise, return the contents of the directory.
        else{
            DIR *d;
            struct dirent *dir;
            d= opendir(path0);
            int temp=1;
            char **dir_content=(char**)malloc(sizeof(char*)*temp+1);

            int j=0;
            if (d){
                while ((dir = readdir(d)) != NULL)
                {
//                    if ( !strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..") )
//                        continue;
                    dir_content=(char**) realloc(dir_content,(temp+j)* sizeof(char *));
                    if(dir_content==NULL){
                        error_500(response, NULL, sd);
//                        perror("\nERROR with malloc\n");
                        free(response);
                        response=NULL;
                        free(dir_content);
//                        close(*sd);
                        exit(1);
                    }
                    dir_content[j]=(char*) malloc(sizeof(char )*((int)strlen(dir->d_name)+1));
                    if(dir_content[j]==NULL){
//                        perror("\nERROR with malloc\n");
                        error_500(response, NULL, sd);
                        free(response);
                        response=NULL;
                        free(dir_content[j]);
                        free(dir_content);
//                        close(*sd);
                        exit(1);
                    }
                    strcpy(dir_content[j],dir->d_name);
                    dir_content[j][(int) strlen(dir->d_name)]=0;
                    j++;
                }
                closedir(d);
            }
            response=(char*)realloc(response,sizeof(char)* strlen(OK_200)+1);
            if(response==NULL){
                perror("\nERROR with malloc\n");
                free(response);
                response=NULL;
                for(int k=0; k<j ;k++)
                    free(dir_content[k]);
                free(dir_content);
                close(*sd);
                exit(1);
            }
            strcat(response, OK_200);
            response[strlen(OK_200)]=0;
            char modified_buffer[128];
            strftime(modified_buffer, sizeof(modified_buffer), RFC1123FMT, localtime(&stat1.st_mtime));
            dir_response(response, path0,dir_content, j, modified_buffer, sd );
            close(*sd);
        }
        return;
    }
    /*
     * If the path is a file
     * (a) if the caller has no 'read' permissions, send a “403 Forbidden” response.
     *     The file must have read permission for everyone and if the file is in some directory, all the directories in the path must have executing permissions.
     * (b) Otherwise, return the file  */

    //If this is not a regular file and the caller has no read permissions

    if( S_ISREG(stat1.st_mode) && (!(stat1.st_mode&S_IROTH))){//If true, so this file has no read permissions.
        response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_403)+1);
        if(response==NULL){
            perror("\nERROR with malloc\n");
            free(response);
            response=NULL;
            close(*sd);
            exit(1);
        }
        strcat(response, BAD_REQUEST_403);
        response[strlen(BAD_REQUEST_403)]=0;
        bad_errors(response,path, sd);
        return;
    }

    /*If I got this line so this is a regular file and the caller has read permissions.
    * Now,I want to be sure that the directories in the path have permissions*/
    if(!have_permissions(path)){//false so there is no permission
        //If I am here so there are no permissions for this directory.
        response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_403)+1);
        if(response==NULL){
            perror("\nERROR with malloc\n");
            free(response);
            response=NULL;
            close(*sd);
            exit(1);
        }
        strcat(response, BAD_REQUEST_403);
        response[strlen(BAD_REQUEST_403)]=0;
        bad_errors(response,path, sd);
        return;
    }
    //If I am here so there are  permissions.
    response=(char*)realloc(response,sizeof(char)* strlen(OK_200)+1);
    if(response==NULL){
        perror("\nERROR with malloc\n");
        free(response);
        response=NULL;
        close(*sd);
        exit(1);
    }
    strcat(response, OK_200);
    response[strlen(OK_200)]=0;
    fileContent(response, path, sd);

}
//------------------bad_errors-------------------
void bad_errors(char *response, char *path, int * sd){
    char* bad_err="";
    char* note="";
    char * cont_len=NULL;
    int flag_400=0;
    int flag_302_403=0;
    if(path==NULL && strstr(response,"500")!=NULL){
        bad_err = "500 Internal Server Error";
        cont_len="144";
        note="Some server side error. ";
    }
    else if(strstr(response, "400")!=NULL) {
        bad_err = "400 Bad Request";
        cont_len="113";
        note="Bad Request.";
        flag_400=1;
    }
    else if(strstr(response, "501")!=NULL) {
        bad_err = "501 Not supported";
        cont_len="129";
        note="Method is not supported.";
    }
    else if(strstr(response,"404")!=NULL) {
        bad_err = "404 Not Found";
        cont_len="112";
        note="File not found.";
    }
    else if(strstr(response,"302")!=NULL){
        bad_err = "302 Found";
        cont_len="123";
        note="Directories must end with a slash.";
        flag_302_403=1;
    }
    else if(strstr(response,"403")!=NULL){
        bad_err = "403 Forbidden";
        cont_len="111";
        note="Access denied.";
//        flag_302_403=1;
    }
    time_t now;
    char timebuf[128]="";//timebuf holds the correct format of the current time.
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    int len= (int)strlen(response)+(int)strlen(LOCATION)+ (int)strlen(timebuf)+ (int) strlen(bad_err)+ (int)strlen(BODY)+ (int)strlen(H4)+2000;
    response=(char*) realloc(response, sizeof(char)*len +1);
    if(response==NULL){
        perror("\nERROR with malloc\n");
        free(response);
        response=NULL;
        close(*sd);
        exit(1);
    }
    strcat(response,timebuf);
    if(flag_302_403==1){
        strcat(response, LOCATION);
        strcat(response, path);
        strcat(response, "/");
    }
    strcat(response,CONTENT_TYPE);
    strcat(response, cont_len);
    strcat(response, CONNECTION_CLOSE);
    strcat(response, TITLE);
    strcat(response, bad_err);
    strcat(response,BODY);
    if(flag_400==1){
        strcat(response, "400 Bad request");
    }
    if(flag_400==0){
        strcat(response, bad_err);
    }
    strcat(response, H4);
    strcat(response, note);
    strcat(response, END_BODY);
    response[strlen(response)]=0;
    int rw=0, sent=0;
    while (rw< strlen(response)){
        if((rw= write(*sd,(response+rw),(int)strlen(response)-rw ))<=0){
            break;
        }
        sent+=rw;
    }
    free(response);
    response=NULL;
    close(*sd);
}
//------------------bad_errors-------------------
//------------------OK response------------------
void ok_response(unsigned char * file_content,char* response, char *path, int file_len,  char *modified ,int *sd){
    time_t now;
    char timebuf[128];//timebuf holds the correct format of the current time.
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char *ext= get_mime_type(path);
    int len= (int)file_len+(int)strlen(path)+(int)strlen(LOCATION)+ (int)strlen(timebuf)+ (int)strlen(BODY)+ (int)strlen(H4)+200;
    response=(char*) realloc(response, sizeof(char)*len +1);
    if(response==NULL){
        perror("\nERROR with malloc\n");
        free(response);
        response=NULL;
        close(*sd);
        exit(1);
    }
    strcat(response,timebuf);
    if(ext!=NULL){
        strcat(response, "\r\nContent-Type: ");
        strcat(response,ext);
    }
    strcat(response,"\r\nContent-Length: ");
    char int_str[20];
    sprintf(int_str, "%d", file_len);
    strcat(response,  int_str);
    strcat(response, "\r\nLast-Modified: ");
    strcat(response, modified);
    strcat(response, CONNECTION_CLOSE);
    int rw=0, sent=0;
    while (rw< strlen(response)){
        if((rw= write(*sd,(response+rw),(int)strlen(response)-rw ))<=0){

            break;
        }
        sent+=rw;
    }
    write(*sd, file_content, file_len);
    free(response);
    close(*sd);
}
//------------------OK response------------------
//-----------------Dir response------------------
void dir_response(char* response, char *path0,char **dir_content, int dirs_amount,  char *modified ,int *sd){
    time_t now;
    char timebuf[128];//timebuf holds the correct format of the current time.
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    int len= (int)strlen(path0)+(int)strlen(TABLE_DIR)+(int)strlen(DIR_H4)+(int)strlen(DIR_START)+(int) strlen(DIR_TITLE)+(int)strlen(LOCATION)+ (int)strlen(CONTENT_TYPE)+(int)strlen(timebuf)+ (int)strlen(BODY)+ (int)strlen(H4)+200;
    response=(char*) realloc(response, sizeof(char)*len +1);
    if(response==NULL){
        perror("\nERROR with malloc\n");
        free(response);
        response=NULL;
        close(*sd);
        exit(1);
    }
    strcat(response,timebuf);
    strcat(response, CONTENT_TYPE);

    int len_to_malloc=0;
    int files_size=0;
    char **response_table=(char**)malloc(sizeof (char *)*dirs_amount+1);
    for(int i=0 ;i <dirs_amount ; i++){
        struct stat stat1;
        len_to_malloc=(int) strlen(ENTITY0)+(int) strlen(RFC1123FMT)+(int) strlen(ENTITY1)+(int) strlen(ENTITY2)+(int) strlen(ENTITY3)+(int) strlen(ENTITY4)+(int) strlen(dir_content[i])+1000;
        response_table[i]=(char *) malloc(sizeof(char)*(len_to_malloc+1));
        strcpy(response_table[i],ENTITY0);
        strcat(response_table[i], dir_content[i]);
        strcat(response_table[i],ENTITY1);
        strcat(response_table[i],dir_content[i]);
        strcat(response_table[i],ENTITY2);
        char *array=(char*)malloc(sizeof (char)*((int) strlen(path0)+ (int) strlen(dir_content[i])+1));
        if(array==NULL){
            perror("\nERROR with malloc\n");
            free(response);
            response=NULL;
            close(*sd);
            exit(1);
        }
        strcpy(array, path0);
        strcat(array,dir_content[i]);
        array[strlen(array)]=0;
        char modify[128] = "";
        if(!stat(array, &stat1))
            strftime(modify, sizeof(modify), RFC1123FMT, localtime(&(stat1.st_ctime)));
        strcat(response_table[i], modify);
        strcat(response_table[i], ENTITY3);
        if(!stat(array, &stat1)&&S_ISREG(stat1.st_mode)){
            size_t file_size=stat1.st_size;
            char content_len[40];
            sprintf(content_len, "%zu", file_size);
            strcat(response_table[i],content_len);
        }
        strcat(response_table[i], ENTITY4);
        response_table[i][(int) strlen(response_table[i])]=0;
        files_size+=(int)strlen(response_table[i]);
        free(array);
        array=NULL;
    }
    files_size+=(int)strlen(DIR_START)+ (int) strlen(path0) +(int)strlen(DIR_TITLE)+(int)strlen(path0)+(int)strlen(H4)+(int)strlen(TABLE_DIR) + (int)strlen(END_DIR);
    char int_str[20];
    sprintf(int_str, "%d", files_size);
    strcat(response, int_str);
    strcat(response, "\r\nLast-Modified: ");
    strcat(response, modified);
    strcat(response,CONNECTION_CLOSE);
    strcat(response, DIR_START);
    strcat(response, path0);
    strcat(response,DIR_TITLE);
    strcat(response, path0);
    strcat(response, H4);
    strcat(response, TABLE_DIR);
    response=(char*) realloc(response, sizeof(char)*(len+files_size) +1);
    if(response==NULL){
        perror("\nERROR with malloc\n");
        free(response);
        response=NULL;
        close(*sd);
        exit(1);
    }
    for(int i=0; i< dirs_amount; i++){
        strcat(response, response_table[i]);
    }
    strcat(response, END_DIR);

    int rw=0, sent=0;
    while (rw< strlen(response)){
        if((rw= write(*sd,(response+rw),(int)strlen(response)-rw ))<=0){
            break;
        }
        sent+=rw;
    }
    for(int i=0 ; i<dirs_amount ; i++){
        free(dir_content[i]);
        free(response_table[i]);
    }
    free(dir_content);
    free(response_table);
    free(response);
    free(path0);
    close(*sd);
}
//-----------------Dir response------------------
//-----------------permissions-------------------
bool have_permissions(char *path){

    char path_prefix[(int)strlen(path)];
    bzero(path_prefix,(int)strlen(path)+1);
    int i =0;
    for(i=0; i< (int)strlen(path); i++){
        path_prefix[i]=path[i];
    }
    struct stat stat1;
    if(stat(path, &stat1) !=0){
        return true;
    }

    char *sl=NULL;
    if((sl= strtok(path_prefix, "/")) ==NULL){
        return true;
    }
    char check[(int)strlen(path)+1];
//    strcpy(check, "/");
    strcpy(check, sl);
    while(sl!=NULL){
//        stat(check, &stat1);//If this path is a dir
        if (stat(check, &stat1) != 0) {
            // stat failed
            return true;
        }
        if((stat1.st_mode & S_IFDIR)&& !(stat1.st_mode & S_IXOTH) )
            return false;
            //If I got this line so this is a file.
        else if(!(stat1.st_mode & S_IFDIR)&& !(stat1.st_mode & S_IROTH))
            return false;
        if((sl= strtok(NULL, "/")) !=NULL ){
            strcat(check,"/");
            strcat(check,sl);
        }
    }
    return true;
}
//-----------------permissions-------------------
void fileContent(char* response, char *path, int *sd){
    size_t size = 1024;
    struct stat stat1;
    stat(path, &stat1);

    FILE * f = fopen (path, "r");

    if(f==NULL){
        response=(char*) realloc(response, sizeof(char)*1);
        response[0]=0;
        error_500(response, NULL, sd);
        return;
    }

    time_t now;
    char timebuf[128];//timebuf holds the correct format of the current time.
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    unsigned long len=(int)strlen(OK_200)+(2*(int)strlen(timebuf))+(int)strlen(OK_200)+size+(int)strlen(CONTENT_TYPE)+(int)strlen(CONNECTION_CLOSE)+2000;
    response=(char*) realloc(response, sizeof(char)*len +1);
    if(response==NULL){
        perror("\nERROR with malloc\n");
        free(response);
        response=NULL;
        close(*sd);
        exit(1);
    }
    strcat(response, timebuf);
    char *ext= get_mime_type(path);
    if(ext!=NULL){
        strcat(response, "\r\nContent-Type: ");
        strcat(response,ext);
    }
    char int_str[20];
    sprintf(int_str, "%d", (int)stat1.st_size);
    strcat(response,"\r\nContent-Length: ");
    strcat(response,  int_str);
    strcat(response,"\r\nLast-Modified: ");
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, localtime(&(stat1.st_ctime)));
    strcat(response, timebuf);
    strcat(response, CONNECTION_CLOSE);
    write(*sd ,response, (int)strlen(response));


    unsigned char buffer[4000];
    size_t rd=0;

    fseek (f, 0, SEEK_SET);
    while((rd=fread (buffer,sizeof (unsigned char),4000, f))>0){
        write(*sd,buffer, rd);
    }
    fclose(f);
//    free(file_content);
    free(response);
    close(*sd);
}

void error_500(char * response, char *path,int *sd){
    response=(char*)realloc(response,sizeof(char)* strlen(BAD_REQUEST_500)+1);
    if(response==NULL){
        perror("\nERROR with malloc\n");
        free(response);
        response=NULL;
        close(*sd);
        exit(1);
    }
    strcat(response, BAD_REQUEST_500);
    response[strlen(BAD_REQUEST_500)]=0;
    bad_errors(response,path, sd);

}
