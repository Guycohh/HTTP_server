# HTTP server
Authored by Guy Cohen

# Description
The HTTP (Hyper Text Transfer Protocol) server is a software application that is used for communication between web clients and servers. This particular implementation of the HTTP server is a program that constructs and sends an HTTP response to a client based on the client's request. It is important to note that this implementation does not include the full HTTP specification, but only a limited subset of it.

The program consists of two source files, server.c and threadpool.c. The server is responsible for handling connections with clients using TCP. To enable a multithreaded program, the server creates a thread pool, which is a group of threads that are created in advance to handle client connections. When a client connection request is received, it is added to a queue so that an available thread in the pool can handle it.
To use the HTTP server, you can specify the following command line arguments:
" server <port> <pool-size> <max-number-of-request> "

port: The port number that your server will listen on.
pool-size: The number of threads in the pool.
max-number-of-request: The maximum number of requests that your server will handle before it destroys the pool."

# Program DATABASE
This program uses dynamic arrays as its main databases. The program will allocate these arrays as needed, with the most important one being the response array, which will be allocated while the program is running.

# Program flow
The program will perform the following actions:

1. Parse the command line arguments.
2. Connect to the server.
3. Construct an HTTP request based on the options specified in the command line.
4. Send the HTTP request to the server.
5. Receive an HTTP response.
6. Display the response on the screen.

This program will implement this two files:
 threadpool.c: This file contains the implementation of the methods from the header file 'threadpool.h'. So that different actions can be made on the threadpool, such as initialization, dispatch, dowork, destroy and various actions on the job queue
server.c: The file 'server.c' contains the main function which receives an array of arguments. The first argument is the port number, followed by the number of threads and the number of requests that the server can handle. After checking the input, the server opens a socket and uses the threadpool to handle HTTP requests from clients. Depending on the type of request, such as an error or a request for a folder or file, the server returns a reply to each client through the socket.

# Functions
-int main(int argc, char **argv)—> This function is the main function of an HTTP server program. It first checks that the command line arguments are valid, parsing the port number, pool size, and maximum number of requests from the arguments. It then creates a thread pool with the specified pool size, creates a socket and binds it to the specified port to listen for client connections, and listens for and accepts client connections. For each accepted client connection, the function uses the thread pool to dispatch a response function to handle the request. When the maximum number of requests has been reached, the function destroys the thread pool and closes the welcome socket.

-bool isnumber (char* str)--> checks if string is a number.

-int responseFunction(void * arg)—>This function gets a client sd and handle it. This function is responsible on "analyze" the response.

-void check_path(char* response, char *path, int *sd)—>The check_path function is a utility function that is used to analyze a file path, check the permissions for the file or directory, and construct an HTTP response based on the analysis and permissions.

-void bad_errors(char *response, char *path, int * sd)—> Create an error message depending on the problem, and write to the socket.

-void fileContent(char* response, char *path,int *sd)—> return the content of a file and write it to socket.

-bool have_permissions(char *path)—>return true if there are permissions.

# Output
The bad_errors function is a utility function that is used to handle various error conditions that may arise when processing an HTTP request. It reads the request from the socket specified by the sd argument, and checks the input to ensure that it is in the correct format. If the request is invalid, the function sends a "400 Bad Request" error message to the client. If the request uses a method other than GET, the function sends a "501 not supported" error message. If the requested path does not exist, the function sends a "404 Not Found" error message. If the path is a directory but does not end with a '/', the function sends a "302 Found" response and includes the original path + '/' in the location header. If the path is a directory and ends with a '/', the function searches for index.html. If it exists, it is returned to the client, otherwise the contents of the directory are returned in a specific format. If the path is a file, the function checks the read permissions and either sends a "403 Forbidden" error message if permissions are not sufficient, or returns the file to the client in a specific format if permissions are sufficient.
  
# Program Files
server.c, threadpool.c

# How to compile?
compile:    gcc -g -Wall threadpool.c server.c -lpthread -o server
run: ./sever <port> <pool-size> <max-number-of-request>
if you want to connect, use browser of another cmd.





