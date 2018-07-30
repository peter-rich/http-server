//
//	myhttpd.cc
//	
//	Zhanfu Yang	
//	
//	www.zhanfuyang.com
//
//	Homework For Purdue CS50011 Lab7 Web Server
//	
//	Finish List:
//
//	ok 0:   Basic of showing the request.
//	ok 1:	Back to parent directory fucntion.
//	ok 2:	Image of the file.
//	ok 3:	Time.
//	ok 4:	Size.
//	ok 5:	Sort by name, time, or size.
//	ok 6:	Extra credit for excutable file.
//
//
//	To do List:
//
//	7:	You don't have the permission to load or execute for some files.
//	8:	Check for other bugs.

//
//	First of all, introduce the usage of the myhttpd if
// the user misused the runfile.
//
const char * usage =
"                                                               \n"
"usage: myhttpd [-f|-p] [<port>]                                \n"
"								\n"
"	1024 < port < 65536					\n"
"                                                               \n"
"	-f: Create a new process for each request 	        \n"
"       							\n"
"	-p: Pool of processed			                \n"
"                                                               \n"
"                                                               \n";

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

int QueueLength = 20;

int debug = 1; // Debug message 

int Concurrency = 0; // 0 = none, 1 = [-f] process based, 2 = [-p] thread pool based

void processRequest(int socket);
void poolSlave(void * p);

void show_file(int socket, struct dirent *filename, char *cwd, struct stat s_buf);
void back_parent(int socket, char* cwd, struct stat s_buf);
 
void html_head(int socket);
void html_back(int socket);
void sort_name_time_size(int case1, int dir_length, long * mod_time, long * file_size, struct dirent ** name);


pthread_mutex_t mt;
pthread_mutexattr_t mattr;

int port = 0;

int main( int argc, char **argv) {
	//Print usage if not enough arguments
	if( argc > 3 || argc < 2 || (argc > 1 && argv[1][0] == '-' && argc < 3)){
		fprintf(stderr, "%s", usage);
		exit(-1);
	}

	// Get the concurrency type from the arguments
	if (argc > 1 && argv[1][0] == '-'){
		if (argv[1][1] == 'f'){
			Concurrency = 1;
			if (debug == 1) printf("Using concurrency mode: process based. \n");
		}
	
		else if (argv[1][1] == 'p'){
			Concurrency = 2;
			if (debug == 1) printf("Using concurrency mode: thread pool based.\n");		
		}

		port = atoi(argv[2]);
		
	}
	else{
		Concurrency = 0;
		if (debug == 1) printf("Using concurrency mode: none.\n");
		port = atoi(argv[1]);	
	}
	if (port < 1024 || port > 65535) {
        	fprintf(stderr, "%s", usage);
        	exit(1);
    	}

	// Set the IP address and port for this server
	struct sockaddr_in serverIPAddress;
	memset(&serverIPAddress, 0, sizeof(serverIPAddress));
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);
	
	// Allocate a socket
	int masterSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (masterSocket < 0){
		perror("socket");
		exit(-1);
	}
	
	// Set socket options to reuse port. Otherwise we will
	// have to wait about 2 minutes before reusing the sae port number
	int optval = 1;
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR,
	(char*) &optval, sizeof(int));
	
	// Bind the socket to the IP address and port
	int error = bind(masterSocket,
		(struct sockaddr *)&serverIPAddress,
		sizeof(serverIPAddress));	
	if (error){
		perror("bind");
		exit(-1);	
	}
	
	// Put socket in listening mode and set the 
	// size of the queue of unprocessed connections
	error = listen(masterSocket, QueueLength);
	if (error){
		perror("listen");
		exit(-1);
	}
	
	if (debug == 1) printf("###Complete.\n");
	if (debug == 1) printf("###\n");

	//
	// Case 1: in the 3 situation wich is -p
	//	
	if (Concurrency == 2){
		pthread_mutexattr_init(&mattr);
		pthread_mutex_init(&mt, &mattr);
		
		pthread_t tid[20];
		pthread_attr_t attr;

		pthread_attr_init(&attr);
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	
		for(int i = 0; i < 20; i ++){
			pthread_create(&tid[i], &attr, (void * (*)(void *))poolSlave, ((void *) (intptr_t) masterSocket));
		}	

		pthread_join(tid[0], NULL);	
	
	}
	else{
		while(1){
			// Accept incoming connections
			struct sockaddr_in clientIPAddress;
			int alen = sizeof(clientIPAddress);
			int slaveSocket = accept(masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);
			
			if (slaveSocket < 0 ){
				//if (errno == EINTR){
				//	continue;
				//}
				
				perror("accept");
				exit(-1);
			}

			//
			//	Case 2: int the first situation: -f
			//	
			if (Concurrency == 1){   
				pid_t slave = fork();
				
				if (slave == 0){
					// Process request.
					processRequest(slaveSocket);
					
					// Close socket
					close(slaveSocket);
		
					exit(1);	
				}
				
				// Close socket
				close(slaveSocket);
			}
			else{	// 
				// Case 3: in the 0 situation wich in the normal mode.
				// with singal thread. 
				//
            			if ( slaveSocket < 0 ) {
              				perror( "accept" );
            		  		exit( -1 );
            			}
				processRequest(slaveSocket);
				
				// Close socket
				close(slaveSocket);
			}		
		}		
	}
}

//
//	The head of the request when browsing the directory.
//
void html_head(int socket){
	const char * head = "HTTP/1.1 200 OK";
	const char * server = "Server: Mattserv/1.0";
	const char * contentTitle = "Content-Type: ";
	const char * crlf = "\r\n";
	write(socket, head, strlen(head));
	write(socket, crlf, strlen(crlf));
	write(socket, server, strlen(server));
	write(socket, crlf, strlen(crlf));
	write(socket, crlf, strlen(crlf));
	
	const char * str = "\
	<!DOCTYPE HTML PUBLIC	\"-//IETF//DTD HTML//EN\">\n \
	<head>\n\
    		<title>CS50011: HTTP Server</title>\n\
 	</head>\n\
  	<body>\n\
    		<h1>CS50011: HTTP Server</h1>\n\
	<table>\n\
	<tr><th valign=\"top\"><img src=\"/icons/blank.gif\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O=A\">Name</a></th><th><a href=\"?C=M;O=A\">Last modified</a></th><th><a href=\"?C=S;O=A\">Size</a></th><th></tr>\
    	<ul>\n";
	write(socket, str, strlen(str));

}

//
//	The back of the request when browsing the directory.
//
void html_back(int socket){
	const char * str = "\
	</ul>\n \
    	<hr>\n \
	</table>\n \
	<hr>\n \
    	<address><a href=\"http://www.zhanfuyang.com\">Zhanfu Yang: yang1676@purdue.edu</a></address>\n\
	<!-- Created: Sat July  28 14:14:47 EST 2018 -->\n\
	<!-- hhmts start -->\n\
	Last modified: Sat July 28 14:14:47 EST 2018\n\
	<!-- hhmts end -->\n\
 	</body>\n\
	</html>\n\
	"; 
	write(socket, str, strlen(str));
	
}

//
//	Show each file's detail information in the web server.
//

void show_file(int socket, struct dirent *filename, char *cwd, struct stat s_buf) {
	/*To judge whether the path is a dir or file.*/
	char file_path[200];
	bzero(file_path,200);
	strcat(file_path,cwd);
	strcat(file_path,"/");
	strcat(file_path,filename->d_name);
			
	/* Input the path information to the s_buf*/
	stat(file_path,&s_buf);
	if (debug == 1) printf("File->%s\n", filename->d_name);
	char doc_path[256];
	int k = 0;
 	while(file_path[k]!='\0') {
		if(file_path[k] == 'o' && file_path[k+1]=='t'
		&& file_path[k+2] == '-' && file_path[k+3] == 'd'
		&& file_path[k+4] == 'i' && file_path[k+5] == 'r'
		&& file_path[k+6] == '/'){
			k = k + 6;
			int org = k;
			while(file_path[k]!='\0') {
				doc_path[k-org]=file_path[k];
				k++;
			}	
			doc_path[k-org]='\0';	
			break;
		}
		k++;
	}
        int i = strlen(doc_path)-1;
        while (i>=0){
                if (doc_path[i]=='/' && doc_path[i+1] == '/') {
                        while(doc_path[i+1] != '\0') {
                                doc_path[i] = doc_path[i+1];
                                i++;
                        }
                        doc_path[i] = '\0';
                        break;
                }
                i--;
        }

	/* Judge whether it's a directory*/
	if(S_ISDIR(s_buf.st_mode) && filename->d_name[0] != '.'){
		const char * str = "<tr><td valign=\"top\">\
          <img src=\"/icons/folder.gif\" alt=\"[PARENTDIR]\"></td><td>\
                <A HREF=\"";
		write(socket, str, strlen(str));
		write(socket, doc_path, strlen(doc_path));
		const char * str2 = "\">";
		write(socket, str2, strlen(str2));
		write(socket, filename->d_name, strlen(filename->d_name));
		const char * str3 = "/</A></td><td align=\"right\">";
                const char * str4 = "</td><td align=\"right\">";
                const char * str5 = " - </td></tr>\n";
		char time[256];
		write(socket, str3, strlen(str3));
		struct tm *p;
		p = gmtime(&(s_buf.st_mtime));
		sprintf(time, "%d-%d-%d %d:%d:%d", p->tm_year+1900, p->tm_mon+1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
		write(socket, time, strlen(time));
		write(socket, str4, strlen(str4));
		write(socket, str5, strlen(str5));
	}

	/* Judge whether it's a file*/
	if(S_ISREG(s_buf.st_mode)&& filename->d_name[0] != '.')
	{
		if ( strstr(filename->d_name, ".cc")!=0||strstr(filename->d_name, ".c")!=0||
		strstr(filename->d_name, ".cpp")!=0||strstr(filename->d_name, ".txt")!=0||
		strstr(filename->d_name, ".sh")!=0||strstr(filename->d_name, ".html")!=0){
			const char * str = "<tr><td valign=\"top\">\
                  	<img src=\"/icons/text2.gif\" \
			alt=\"[PARENTDIR]\"></td><td>\
                 	<A HREF=\"";
			write(socket, str, strlen(str));
		}
		else if ( strstr(filename->d_name, ".gif")!=0||
		strstr(filename->d_name, ".xbm")!=0){
			const char * str = "<tr><td valign=\"top\">\
			<img src=\"/icons/image2.gif\" \
                        alt=\"[PARENTDIR]\"></td><td>\
                       	<A HREF=\"";
                        write(socket, str, strlen(str));
		}
		else if ( strstr(filename->d_name, ".tar")!=0||
                strstr(filename->d_name, ".zip")!=0 || strstr(filename->d_name, ".gz")!=0){
                        const char * str = "<tr><td valign=\"top\">\
                        <img src=\"/icons/tar.gif\" \
                        alt=\"[PARENTDIR]\"></td><td>\
                        <A HREF=\"";
                        write(socket, str, strlen(str));
                }

		else{
			const char * str = "<tr><td valign=\"top\">\
                        <img src=\"/icons/unknown2.gif\"\
			alt=\"[PARENTDIR]\"></td><td>\
                        <A HREF=\"";
			write(socket, str, strlen(str));
		}
		write(socket, doc_path, strlen(doc_path));
		const char * str2 = "\">";
		write(socket, str2, strlen(str2));
		write(socket, filename->d_name, strlen(filename->d_name));
		const char * str3 = "</A></td><td align=\"right\">";
		const char * str4 = "</td><td align=\"right\">";
		const char * str5 = "</td></tr>\n";
		char size[256];
		char time[256];
		struct tm *p;
		p = gmtime(&(s_buf.st_mtime));
		sprintf(time, "%d-%d-%d %d:%d:%d", p->tm_year+1900, p->tm_mon+1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
		long file_size = s_buf.st_size;
		if(file_size > 1024) {
			file_size = file_size/1024;
			if(file_size > 1024) {
				file_size = file_size/1024;
				sprintf(size, "%ldM", file_size);
			}
			else{
				sprintf(size, "%ldK", file_size);
			}
		}
		else{
			sprintf(size, "%ld", file_size);
		}
		write(socket, str3, strlen(str3));
		write(socket, time, strlen(time));
		write(socket, str4, strlen(str4));
		write(socket, size, strlen(size));	
		write(socket, str5, strlen(str5));
	}
}

//
//	Show how to go back to 's parent's directory or index.html.
//
void back_parent(int socket, char* cwd, struct stat s_buf) {
		//
		//	Back to parent directory.
		//
		char doc_path[256];
		int k = strlen(cwd)-1;
 		int last_l;
		int checkSort = 0;
		if (strstr(cwd, "?") != 0) {
			checkSort = 1;
		}
		while ( k >= 0 ) {
			if (cwd[k] == '/' ) {
				last_l = k;
				break;
			}
			k--;
		}
		 
		while(k>=0) {
	
			if(cwd[k] == 'o' && cwd[k+1]=='t'
				&& cwd[k+2] == '-' && cwd[k+3] == 'd'
				&& cwd[k+4] == 'i' && cwd[k+5] == 'r'
				&& cwd[k+6] == '/'){
				k = k + 6;
				int org = k;
				while(k < last_l) {
					doc_path[k-org]=cwd[k];
					k++;
				}	
				doc_path[k-org]='\0';	
				break;
			}
			k--;
		}

		int i = strlen(doc_path)-2;
		while (i>=0){
			if (doc_path[i]=='/' && doc_path[i+1] == '/') {
				while(doc_path[i+1] != '\0') {
					doc_path[i] = doc_path[i+1];
					i++;	
				}
				doc_path[i] = '\0';
				break;
                	}
                	i--;
		}
	
		if(doc_path[0]=='\0'){
			const char * str = "<tr><td valign=\"top\">\
                        <img src=\"https://www.cs.purdue.edu/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td>\
			<A HREF=\"/";
			const char * str2 = "\"> Back To Parent Directory </A></td></tr>\n";
			write(socket, str, strlen(str));
			char a[256];
			sprintf(a, "%d", port);
			//write(socket, a, strlen(a));
			write(socket, str2, strlen(str2));
		}
		else {	
			const char * str = "<tr><td valign=\"top\">\
			<img src=\"https://www.cs.purdue.edu/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td>\
			<A HREF=\"";
                	write(socket, str, strlen(str));
			char a[256];
                        sprintf(a, "%d", port);
                        //write(socket, a, strlen(a));
                	write(socket, doc_path, strlen(doc_path));
                	const char * str2 = "\">Back To Parent Directory</A></td></tr>\n";
                	write(socket, str2, strlen(str2));
		}

}

void sort_name_time_size(int case1, int dir_length, long * mod_time, long * file_size, struct dirent ** name){
		struct dirent * filename;	
		long tmp_time,tmp_size;
		int i;
		if (case1 == 1 ) { // Sort By Modify Time.
			for(i = 0; i < dir_length; i ++){
				for(int j = 0 ;j < dir_length-i-1; j ++){
					if (mod_time[j] > mod_time[j+1]) {
						tmp_time = mod_time[j+1];
						mod_time[j+1] = mod_time[j];
						mod_time[j] = tmp_time;
                                                tmp_size = file_size[j+1];
                                                file_size[j+1] = file_size[j];    
                                                file_size[j] = tmp_size;
                                                filename = name[j+1];
                                                name[j+1] = name[j];    
                                                name[j] = filename;
					}
				}
			}
		}	
		else if ( case1 == 2 ) { //Sort By Name.
			for(i = 0; i < dir_length; i ++){
                                for(int j = 0 ;j < dir_length-i-1; j ++){
					//if (debug == 1) printf("%s ", name[j]->d_name);
                                        if (strcmp(name[j]->d_name, name[j+1]->d_name) > 0) {
                                                tmp_time = mod_time[j+1];
                                                mod_time[j+1] = mod_time[j];    
                                                mod_time[j] = tmp_time;
						tmp_size = file_size[j+1];
                                                file_size[j+1] = file_size[j];
                                                file_size[j] = tmp_size;
						filename = name[j+1];
                                                name[j+1] = name[j];        
                                                name[j] = filename;
                                        }
                                }
				//if (debug == 1) printf("\n");
                        }
	
		}
		else if ( case1 == 3 ) { //Sort By Size.
			for(i = 0; i < dir_length; i ++){
				for(int j = 0 ;j < dir_length-i-1; j ++){
					if (file_size[j] > file_size[j+1]) {
						tmp_time = mod_time[j+1];
						mod_time[j+1] = mod_time[j];
						mod_time[j] = tmp_time;
						tmp_size = file_size[j+1];
						file_size[j+1] = file_size[j];
						file_size[j] = tmp_size;
						filename = name[j+1];
						name[j+1] = name[j];
						name[j] = filename;
					}
				}
			}	
		}
}
//
//	processRequest function: inthe singal thread based function	
//
void processRequest(int socket){
	// Buffer used to store the name received from the client
	const int MaxName = 1024;
        char currString[MaxName + 1];
        int n;
        int length = 0;
	int lengthDoc = 0;
        // Currently character read
        unsigned char newChar;

        // Last character read
        unsigned char oldChar = 0;

        // Flag for GET request
        int gotGet = 0;

        // Requested document path
        char docPath[MaxName + 1] = {0};
        int nameDocLength = 0;
        int docDone = 0;

	// Get the path
	while ((n = read(socket, &newChar, sizeof(newChar))) > 0){
                if (newChar == ' '){
                        if (gotGet == 0){
                                gotGet = 1;
                        }
                        else{
                                currString[length++] = 0;
                                docPath[lengthDoc] = 0;
				break;
                        }
                }
                else if(newChar == '\n' && oldChar == '\r'){
                        break;
                }
                else{
                        oldChar = newChar;
                        if (gotGet == 1){
                                currString[length++] = newChar;
				docPath[lengthDoc++] = newChar;
                        }
		}
	}

	// Get the rest of the request.
	while ((n = read(socket, &newChar, sizeof(newChar)))) {
        	currString[length++] = newChar;
		if (newChar == '\n') {
			if (currString[length - 2] == '\r' && currString[length - 3] == '\n'
				&& currString[length - 4] == '\r') {
				break;	
			}
		}
	}

	if (debug == 1){
    		printf("The GET request is: \n%s\n\n", currString);
	}

    	//Mapping the document path to the real path file
    	//char * cwd = (char *) malloc (256 * sizeof(char));
    	char cwd[256] = {0};
    	getcwd(cwd, 256);
	
	//To map the docPath to the root path  "/http-root-dir/" 
    	if ( strstr(docPath, "/icons") != 0
    	 || strstr(docPath, "/htdocs") != 0
	|| strstr(docPath, "/cgi-src") != 0
     	|| strstr(docPath, "/cgi-bin") != 0) {
        	strcat(cwd, "/http-root-dir");
        	strcat(cwd, docPath);
    	} 
	else {
        	if (!strcmp(docPath, "/")) {
            		strcpy(docPath, "/index.html");
            		strcat(cwd, "/http-root-dir/htdocs");
        	    	strcat(cwd, docPath);
        	} else if (!strcmp(docPath, "..")) {
       	     		strcat(cwd, "/http-root-dir/htdocs/");
            		strcat(cwd, docPath);
        	} else {
            		strcat(cwd, "/http-root-dir/htdocs");
            		strcat(cwd, docPath);
        	}
    	}

    	if (strstr(docPath, "..") != 0) {
        	char resolved[MaxName + 1] = {0};
        	char * r = realpath(docPath, resolved);
        	if (r == NULL) {
            		perror("realpath");
            		exit(0);
        	}
    	}

    	char contentType[MaxName + 1] = {0};	

	if (strstr(docPath, ".html") != 0) {
		strcpy(contentType, "text/html");
	}
	else if (strstr(docPath, ".gif") != 0) {
		strcpy(contentType, "image/gif");
	}
	else if (strstr(docPath, ".jpg") != 0) {
		strcpy(contentType, "image/jpeg");
	}
	else {
		strcpy(contentType, "text/plain");
	}

	//
	// Begin to check whether the path is a directory.
	//

	struct stat s_buf; // put the file's information into the stat struct in c.
        if (debug ==1){
                printf("original->cwd:%s\n", cwd);
        }
	if (cwd[strlen(cwd)-1] == '/') {
		cwd[strlen(cwd)-1] = '\0';
	}
	int i = strlen(cwd)-1;
	while (i>=0){
		if (cwd[i]=='/' && cwd[i+1] == '/') {
			while(cwd[i+1] != '\0') {
				cwd[i] = cwd[i+1];
				i++;
			}
			cwd[i] = '\0';
			break;
		}
		i--;
	}
	stat(cwd,&s_buf);
	
	if (debug ==1){
		printf("after->cwd:%s\n", cwd);
	}

	//
	//	Case one: It's a action to sort for time's name's or other.  
	//
		
	if(strstr(cwd, "?C=M;O=A") != 0 || strstr(cwd, "?C=N;O=A") != 0
	|| strstr(cwd, "?C=S;O=A") != 0){
		int case1 = 0;
		if (strstr(cwd, "?C=M;O=A") != 0 ){
			case1 = 1;
		}
		else if (strstr(cwd, "?C=N;O=A") != 0) {
			case1 = 2;
		}
		else if (strstr(cwd, "?C=S;O=A") != 0) {
			case1 = 3;
		}
		//
		//	There is an action.
		//
		int i = strlen(cwd) - 1;
		while(i>=0){
			if(cwd[i] == '?' && cwd[i+1] == 'C') {
				for(int j = i; j < strlen(cwd)-1 ; j++){
					cwd[j]='\0';
				}
				break;
			}
			i--;
		}	
	
		html_head(socket);
		if (debug == 1) printf("->Action->DIR:\n");

		struct dirent * filename;

		DIR *dp = opendir(cwd);
		i = 0;	
		struct dirent * name[256];
				long mod_time[256];
		long file_size[256];

		while(filename = readdir(dp))
		{
			if(filename->d_name[0] != '.') {
				if (debug == 1) printf("->file->%d:\n", i);
				/*To judge whether the path is a dir or file.*/
				char file_path[200];
				bzero(file_path,200);
				strcat(file_path,cwd);
				strcat(file_path,"/");
				strcat(file_path,filename->d_name);
				
			       	name[i] = filename;

				/* Input the path information to the s_buf*/
				stat(file_path,&s_buf);
				mod_time[i] = s_buf.st_mtime;
				if (S_ISDIR(s_buf.st_mode)) {
					file_size[i] = -1;	
				}
				else {
					file_size[i] = s_buf.st_size;
				}
				i++;			
			}
		}
		if (debug == 1) printf("->Begin to sort:%d\n",case1);
		int dir_length = i;
		
		sort_name_time_size(case1, dir_length, mod_time, file_size, name);

		for (i = 0; i < dir_length; i ++){
                        
			show_file(socket, name[i], cwd, s_buf);

		}

		back_parent(socket, cwd, s_buf);	
		html_back(socket);
	}

	//
	//	Case two: It's a cgi-bin which need to run and May have several parameters.
	//

	else if (strstr(cwd, "/cgi-bin/") != 0 && (strstr(cwd, "?")||S_ISDIR(s_buf.st_mode) || S_ISREG(s_buf.st_mode) )) {
		char param[256]={'\0'}, param1[256]={'\0'}, param2[256]={'\0'}; // Initial the two parameters.
		param1[0]='e';param2[0]='e';
		param1[1]='x';param2[1]='x';
		param1[2]='p';param2[2]='p';
		param1[3]='o';param2[3]='o';
		param1[4]='r';param2[4]='r';
		param1[5]='t';param2[5]='t';	
		param1[6]=' ';param2[6]=' ';
		if (strstr(cwd, "?") != 0) { // have parameters
			if (debug == 1) printf("Have all parameters --> %s\n", cwd);
			int i = strlen(cwd)-1;
			int error=0;
			while (i >= 0) {
				if (cwd[i] == '?') {
					i=i+1;
					int l = i;
					error = 3;
					while(cwd[i] != '&') {
						param1[7+i-l] = cwd[i];
						if (cwd[i] == '=') error = 0;
						if (cwd[i] == '\0') {
							error = 1;
							break;
						}
						i++;
					}
					
					param1[7+i-l]='\0';
					i++;
					l=i;
					if (error == 0 ) error = 4;
					while(cwd[i]!='\0') {
						param2[7+i-l] = cwd[i];
						if (cwd[i] == '=') error = 0;
						i++;
					}
					param2[7+i-l]='\0';
					break;
				}	
				i--;
			}
			if (error == 0){
				system(param1);
				system(param2);
				i = 0; 
				int j = 0;
				while(param1[i] != '\0'){
					if(param1[i] == '='){
						i++;
						int l = i;
						while(param1[i] != '\0'){
							param[i-l]=param1[i];		
							i++;
						}
						param[i-l]=' ';
						i = i - l + 1;
						break;
					}
					i++;
				}
                                while(param2[j] != '\0'){
                                        if(param2[j] == '='){
                                                j++;
                                                int l = j;
                                                while(param2[j] != '\0'){
                                                        param[i+j-l]=param2[j];   
                                                        j++;
                                                }
                                                param[i+j-l]='\0';
                                                break;
                                        }
                                        j++;
                                }
			}	
		}
		char script[256];
		int j = strlen(cwd) - 1; 
		while(j >= 0) {
			if (cwd[j] == '/') {
				int length = j;
				while(cwd[j] != '\0' && cwd[j] != '?') {
					script[j-length] = cwd[j];
						j++;
				}
				break;
			}
			j--;
		}

		char out[256];

		if (debug == 1) {
			printf("The parameters are: p1->%s  p2->%s p3->%s\n", param1, param2, param);
		}

		sprintf(out, "./http-root-dir/cgi-bin%s %s > result.out", script, param);
		system(out);

		if (debug == 1) printf("File's Path-> %s\n",out);

		FILE *document;
	
		document = fopen("result.out", "rb");
	
		if (document != NULL ) {
	
			if (debug > 0){
				printf("### document > 0 \n");
			}
			const char * head = "HTTP/1.1 200 OK";
			const char * server = "Server: Mattserv/1.0";
			const char * contentTitle = "Content-Type: ";
			const char * crlf = "\r\n";
			write(socket, head, strlen(head));
			write(socket, crlf, strlen(crlf));
			write(socket, server, strlen(server));
			write(socket, crlf, strlen(crlf));
			//write(socket, contentTitle, strlen(contentTitle));
			//write(socket, contentType, strlen(contentType));
			//write(socket, crlf, strlen(crlf));
			//write(socket, crlf, strlen(crlf));
			if (strstr(contentType, "text/") != 0){
				const char * str = "<html><body>a</body>\n";
				//write(socket, str , sizeof(str));
			}	
		
			long count = 0;
		
			char c;
			while (count = read(fileno(document), &c, sizeof(c))) {
				if (write(socket, &c, sizeof(c)) != count) {
					perror("write");
					if (debug ==1 ){
               	        			printf("#--> Write Error\n");
               				}
					return;
				}
				if (debug > 0){
               	        		printf("%c", c);
               			}
		
			}
			
			if (debug > 0){
               		        printf("### document ok! \n");
               		}
			
			fclose(document);
			system("rm result.out");	
			if (strstr(contentType, "text/") != 0){
               		        const char * str = "</html>\n";
               		        //write(socket, str , sizeof(str));
               		}
	
		}
	}

	//
	//	Case three: It's a directory.
	//

	else if(S_ISDIR(s_buf.st_mode)){
		html_head(socket);
		if (debug == 1) printf("->DIR:\n");
		struct dirent *filename;
		DIR *dp = opendir(cwd);

		while(filename = readdir(dp))
		{
			show_file(socket, filename, cwd, s_buf);
		}
		back_parent(socket, cwd, s_buf);
		html_back(socket);
	}	
	
	//
	//   Case four:
	//
	// If this is a file begin to the second stage to 
	// load the file or download the file.
	//
	else {
		FILE *document;
		if (debug > 0){
			printf("### document begin to load \n");
			printf("FilePath: %s\n", cwd);
		}	
	
		document = fopen(cwd, "rb");
	
		if (document != NULL ) {
	
			if (debug > 0){
				printf("### document > 0 \n");
			}
			const char * head = "HTTP/1.1 200 OK";
			const char * server = "Server: Mattserv/1.0";
			const char * contentTitle = "Content-Type: ";
			const char * crlf = "\r\n";
			write(socket, head, strlen(head));
			write(socket, crlf, strlen(crlf));
			write(socket, server, strlen(server));
			write(socket, crlf, strlen(crlf));
			write(socket, contentTitle, strlen(contentTitle));
			write(socket, contentType, strlen(contentType));
			write(socket, crlf, strlen(crlf));
			write(socket, crlf, strlen(crlf));
			if (strstr(contentType, "text/") != 0){
				const char * str = "<html><body>a</body>\n";
				//write(socket, str , sizeof(str));
			}	
		
			long count = 0;
		
			char c;
			while (count = read(fileno(document), &c, sizeof(c))) {
				if (write(socket, &c, sizeof(c)) != count) {
					perror("write");
					if (debug ==1 ){
                	        		printf("#--> Write Error\n");
                			}
					return;
				}
				if (debug > 0){
                	        	printf("%c", c);
                		}
	
			}
		
			if (debug > 0){
                	        printf("### document ok! \n");
                	}
		
			fclose(document);
		
			if (strstr(contentType, "text/") != 0){
                	        const char * str = "</html>\n";
                	        //write(socket, str , sizeof(str));
                	}
		} 
		else {
			if (debug > 0){
                	        printf("### document not found \n");
                	}
			const char *message = "<tr> <font size=\"12\" color=\"red\">File not found!</font></tr>";
		
			write(socket, "HTTP/1.1 404 File Not Found", 27);
			write(socket, "\r\n", 2);
			write(socket, "Server: Mattserv/1.0", 20);
			write(socket, "\r\n", 2);
			write(socket, "Content-type: text/html", 23);
			write(socket, "\r\n", 2);
			write(socket, "\r\n", 2);
			const char * str = "\
		        <!DOCTYPE HTML PUBLIC   \"-//IETF//DTD HTML//EN\">\n \
		        <head>\n\
        	       		<title>CS50011: HTTP Server</title>\n\
        		</head>\n\
        		<body>\n\
        	        	<h1>CS50011: HTTP Server</h1>\n\
        			<table>\n\
        			<ul>\n";
        		write(socket, str, strlen(str));
			write(socket, message, strlen(message));
			html_back(socket);
		}
	}
}

//
//	poolSlave function: creat a pool of thread when execute the request. 
//
void poolSlave(void * p){
	while(1){
		pthread_mutex_lock(&mt);
		int socket = (int) (intptr_t) p;
		// Accept outside's connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof(clientIPAddress);
		int slaveSocket = accept(socket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);
		//check if accept worked
		
		pthread_mutex_unlock(&mt);
	
		if (slaveSocket < 0){
			if (errno == EINTR){
				continue;
			}
	
			perror("accept");
			exit(-1);
		}
		// Process request
		processRequest(slaveSocket);
		
		// Close socket
		close(slaveSocket);
	}
}

