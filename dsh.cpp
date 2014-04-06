/***************************************************************************//**
 * Operating Systems Program 1 - Introduction to the POSIX programming environment: Diagnostic Shell 
 *
 * Author -  Derek Stotz
 *			 Using example code from Dr. Christer Karlsson for functions reading in from proc directory, especially get_cpu_clock_speed
 *
 * Date - February 2, 2014
 *
 * Instructor - Dr. Karlsson
 *
 * Course - CSC 456
 *
 ******************************************************************************/
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>     
#include <sys/shm.h>
#include <sys/sem.h>


using namespace std;

/*********************/
//Program 3 Functions//
/*********************/

#define SEMKEY 1129 //the key of semaphore 0
#define SHMKEY 1000 //the key of mailbox 0

#define INFOBOXKEY 1128 //the key of the info_box
#define NUMBOXES 128    //the max number of boxes and keys.  mailboxes are referenced as "mailbox 0 - 128"
#define K 1024  //bytes in a kb

#define READ_WRITE 0666

// This is defined in linux/sem.h
// Included here....
union semun 
{
  int              val;       /* value for SETVAL */
  struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
  unsigned short int *array;  /* array for GETALL, SETALL */
  struct seminfo *__buf;      /* buffer for IPC_INFO */
};


//The function to setup the infobox, if it does not already exist
bool create_infobox()
{
    if ((shmget(INFOBOXKEY, 10*K, READ_WRITE | IPC_CREAT | IPC_EXCL)) < 0)  //0666 permits read and write
       {
          return false;
       }
   return true;
}

//lock a semaphore. Return success
bool lock_sem(int key)
{
    union semun options;

    // Using SEMKEY, create one semaphore with access permissions 0666:
    int id = semget(key + SEMKEY, 1, IPC_CREAT | IPC_EXCL | READ_WRITE);

     options.val = 1;
     semctl(id , 0, SETVAL, options); 
      
      // Test that the semaphore was created correctly:
     if (semctl(id, 0, GETVAL, 0) ==0 ) {
       return false;
     }
     return true;
}

//unlock a semaphore. Return success
bool unlock_sem(int key)
{
    union semun options;

    // Using SEMKEY, create one semaphore with access permissions 0666:
    int id = semget(key + SEMKEY, 1, IPC_CREAT | IPC_EXCL | READ_WRITE);

     options.val = 0;
     semctl(id , 0, SETVAL, options); 
      
      // Test that the semaphore was created correctly:
     if (semctl(id, 0, GETVAL, 0) ==0 ) {
       return false;
     }
     return true;
}

//returns true if the semaphore is unlocked.  Returns false if it's locked
bool locked(int key)
{
    int id = semget(key + SEMKEY, 1, READ_WRITE);
    return semctl(id, 0, GETVAL, 0);    //1 is locked, 0 is unlocked
}

//creates a new semaphore with a specified key. Returns the id, or -1 if unsuccessful
int create_sem(int key)
{
  union semun options;

  // Using SEMKEY, create one semaphore with access permissions 0666:
  int id = semget(key + SEMKEY, 1, IPC_CREAT | IPC_EXCL | READ_WRITE);
  
  // Initialize the semaphore at index 0
  options.val = 1;
  semctl(id , 0, SETVAL, options); 
  
  // Test that the semaphore was created correctly:
  if (semctl(id, 0, GETVAL, 0) ==0 ) {
    return -1;
  }
  return id;
}

bool del_sem(int key)
{
    int id = semget(key + SEMKEY, 1, IPC_CREAT | IPC_EXCL | READ_WRITE);
    semctl(id, 0, IPC_RMID, 0);
}

//creates a new shared memory with a specified key and size in kb. Returns the id, or -1 if unsuccessful
int create_shm(int key, int size)
{
    int id = -1;

    if ((id = shmget(key + SHMKEY, K * size, READ_WRITE | IPC_CREAT | IPC_EXCL)) < 0)  //0666 permits read and write
       {
          return -1;
       }
   return id;
}

bool del_shm(int id, int size)
{
    shmctl(id, IPC_RMID, 0);
}

//writes data to the shared memory at shmkey, truncating it if it runs out of space
void write_shm(int shmid, int maxsize, string data)
{
  if ( shmid < 0)
  {
    return;
  }  

  //attach the shared memory to process:
  char *addr =  (char*)shmat(shmid, 0, 0);

  //build a return string
  string output = "";
  
  int i;
  for(i = 0; (i < data.size()) && (i < (maxsize - 1)); i++ )
  {
      *(addr + i) = data[i];
  }
  *(addr + i) = '\0';
 
}

//returns the contents of a shared memory segment
string read_shm(int shmid)
{
  if ( shmid < 0)
  {
    return "e";
  }  

  //attach the shared memory to process:
  char *addr =  (char*)shmat(shmid, 0, 0);

  //build a return string
  string output = "";
  
  int i = 0;
  while(*addr != '\0')
  {
    output.push_back(*(addr + i));
    i++;
  }
  
  return output;
  
}

int mexample_main()
{
int shmid; 
  int i;
  int pid;
  int opid;
  int *pint;
  char *addr;

  printf ("A shared memory test\n");

  // Using SHMKEY, create one shared memory region with access permissions 0666:
  shmid = shmget(SHMKEY, 10*K, IPC_CREAT | IPC_EXCL | READ_WRITE);

  printf ("Shared memory id = %d\n", shmid);
  if ( shmid < 0)
  {
    printf("***ERROR: shmid is %d\n", shmid);
    perror("shmget failed");
    exit(1);
  }  

  //attach the shared memory to process:
  addr =  (char*)shmat(shmid, 0, 0);
  printf("addr 0x%x\n", addr);

  // Setup a pointer to address an array of integers:
  pint = (int *) addr;


  printf("Prior to fork\n");
  pid = fork();

  if (pid == 0) {
    printf("In child, after fork\n");
    printf("Sleep for 2 seconds while parent writes data\n");

    sleep(2);
    // Read data back and write to stdout:
    for (i=0;i<128;i++)  
      printf("index = %d\t value = %d\n", i, *(pint + i));
  
  } else {
    printf("In parent, after fork\n");

    // Write data into shared memory block:
    for (i=0;i<128;i++)  {
      *(pint + i) = 128 - i;
      printf(".");
    }
    printf("\n");
    exit(0);

  }
    printf("\n");

  shmctl(shmid, IPC_RMID, 0);

}



int sexample_main()
{
  int id; 
  int i;
  int opid;
  
  struct sembuf lock;  
  union semun options;
  
  printf ("A semaphore test\n");

  // Using SEMKEY, create one semaphore with access permissions 0666:
  id = semget(SEMKEY, 1, IPC_CREAT | IPC_EXCL | READ_WRITE);
  printf ("Semaphore id = %d\n", id);
  
  // Initialize the semaphore at index 0
  options.val = 1;
  semctl(id , 0, SETVAL, options); 
  
  // Test that the semaphore was created correctly:
  if (semctl(id, 0, GETVAL, 0) ==0 ) {
    printf ("can not lock semaphore.\n");
    exit(1);
  }
  
  // print the value of the semaphore:
  i = semctl(id, 0, GETVAL, 0);
  printf ("Value of semaphore at index 0 is %d\n", i);
  
  // Set the semaphore:
  lock.sem_num = 0;  // semaphore index
  lock.sem_op = -1; // the operation
  lock.sem_flg = IPC_NOWAIT;  // operation flags
  opid = semop(id, &lock, 1);  // perform the requested operation
  
  // signal if an error occurred
  if (opid == -1 ) {
    printf ("can not lock semaphore.\n");
    exit(1);
  }
  
  // print the value of the semaphore
  i = semctl(id, 0, GETVAL, 0);
  printf ("Value of semaphore at index 0 is %d\n", i);
  
  // Unset the semaphore:
  lock.sem_num = 0; // semaphore index
  lock.sem_op = 1; // the operation
  lock.sem_flg = IPC_NOWAIT; // operation flags
  opid = semop(id, &lock, 1); // perform the requested operation
  
  // signal if an error occured
  if (opid == -1 ) {
    printf ("can not unlock semaphore.\n");
    exit(1);
  }
  
  // print the value of the semaphore
  i = semctl(id, 0, GETVAL, 0);
  printf ("Value of semaphore at index 0 is %d\n", i);
  
  // remove the semaphore
  semctl(id, 0, IPC_RMID, 0);
  return (0);
  }


//command_mboxwrite
//
// opens up a mailbox for writing
//
//mailbox - the given mailbox number
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_mboxwrite(int[] sizes, int[] id, int mailbox, ostream& cout)
{
    bool success = false;

    //fork

    //check status of the associated semaphore
    //if it is locked, wait for it to be unlocked
    //if it is unlocked, lock it and continue
    
    //connect to the mailbox and write to it
    //exit the connection
    //unlock the semaphore
    //success = true
    
    //join

    if(success)
        return true;
        
    return false;
}

//command_mboxread
//
// opens up a mailbox for reading
//
//mailbox - the given mailbox number
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_mboxread(int[] sizes, int[] id, int mailbox, ostream& cout)
{
    bool success = false;

    //fork

    //connect to the mailbox and read from it
    //exit the connection
    //success = true
    
    //join

    if(success)
        return true;
        
    return false;
}

//command_mboxdel
//
// deletes all mailboxes and semaphores
//
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_mboxdel(ostream& cout)
{
    bool success = false;

    //fork

    //for every semaphore/shared memory
    
    //delete the shared memory and semaphore
    //success = true
    
    //join

    if(success)
        return true;
        
    return false;
}

//command_mboxinit
//
// sets up a given number of mailboxes of a given size
//
//num_mailboxes - the number of mailboxes to set up
//mailbox_size - the size of each mailbox in kb
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_mboxinit(int[] sizes, int[] id, int num_mailboxes, int mailbox_size, ostream& cout)
{
    bool success = false;

    //fork

    //for i from 0 to num_mailboxes - 1
    //create a shared memory segment of size mailbox_size and a semaphore
    
    //success = true
    
    //join

    if(success)
        return true;
        
    return false;
}

//command_mboxcopy
//
// copy the contents of one mailbox into another
//
//mailbox1 - the mailbox to copy from
//mailbox2 - the mailbox to copy to
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_mboxcopy(int[] sizes, int[] id, int mailbox1, int mailbox2, ostream& cout)
{
    bool success = false;
    string copy_string = "";
    
    
    //fork

    //connect to the mailbox1 and store its contents in a string
    
    //check status of the associated semaphore for mailbox2
    //if it is locked, wait for it to be unlocked
    //if it is unlocked, lock it and continue
    
    //connect to the mailbox2 and write to it
    //exit the connection
    //unlock the semaphore
    
    //join

    if(success)
        return true;
        
    return false;
}



/********************/
// Utility Functions//
/********************/

//create shared a shared memory mailbox


//if this is ever set to true at the beginning of the event loop, return from main.
static int exiting = false;

//catch
//
// gives output related to a caught signal
//
//param n - the signal number caught
extern void catch_signal (int n)
{
  	printf ("A signal was caught.  Signal = %d\n\n", n);
}


//get_cpuinfo
//
// Reads in and displays some cpu info from proc/cpuinfo
//
//returns - whether or not the function executed successfully
bool get_cpuinfo(ostream& cout)
{
	  ifstream fin;
	  char buffer[1024];
	  char* match;
          string cpu_string;

	  // Read the entire contents of /proc/cpuinfo into the buffer.
	  fin.open("/proc/cpuinfo", ios::in);

	if (!fin)
	{
		 return false;
	}

	printf("CPU INFO:\n\n");
	bool printing = false;

	do{
		fin.getline(buffer, sizeof buffer);
		cpu_string = string(buffer) + "\n";

		if(!printing && cpu_string.find("cpu MHz"))
			printing = true;
		if(printing)
			printf(cpu_string.c_str());

	}while(cpu_string.find("cache size") == -1);

	printf("\n");

	return true;
}


//is_integer
//
// returns whether or not the given string is an integer or not.
//
//str - the given string to be evaluated
//
//returns - whether or not the string can be parsed into an integer value
bool is_integer(string str)
{
	if(str.empty())
		return false;

    string::const_iterator ch_it = str.begin();
    while (ch_it != str.end())
		{
			if (!isdigit(*ch_it))
				return false;
			ch_it++;
		}
    return true;
}



//get_uptime
//
// gets the uptime information sored in proc/uptime
//
//cout - the ostream to display through
//total_seconds, idle_seconds - the passed-by-reference variables to save the results to
//
//returns - whether or not there was success
bool get_uptime(ostream& cout, float &total_seconds, float &idle_seconds)
{
	/*
	The first number is the total number of seconds the system has been up. The second number is how much of that time the machine has spent idle, in seconds.
	*/

	ifstream fin;
	string filepath = "/proc/uptime";

	// Read the entire contents of /proc/uptime into the buffer.
	fin.open(filepath, ios::in);

	if (!fin)
		return false;

	//read from the file


	fin >> total_seconds;
	fin >> idle_seconds;

	fin.close();

	return true;
}



//get_linuxinfo
//
// displays the linux version information stored in proc/
//
//
//returns - whether or not there was success
bool get_linuxinfo()
{
	char buffer[1024];

	ifstream fin;
	string filepath = "/proc/version";

	// Read the entire contents of /proc/version into the buffer.
	fin.open(filepath, ios::in);

	if (!fin)
		return false;

	//read from the file
	fin.getline(buffer, sizeof(buffer), '#');

	printf("\nVERSION INFO:\n\n");
	printf(buffer);
	printf("\n\n");

	fin.close();

	return true;
}


//get_meminfo
//
// displays the memory total and memory free stored in proc/meminfo
//
//
//returns - whether or not there was success
bool get_meminfo()
{
	char buffer[1024];

	ifstream fin;
	string filepath = "/proc/meminfo";

	// Read the entire contents of /proc/meminfo into the buffer.
	fin.open(filepath, ios::in);

	if (!fin)
		return false;

	//read from the file

	printf("MEMORY INFO:\n\n");

	fin.getline(buffer, sizeof(buffer));
	printf(buffer);
	printf("\n");

	fin.getline(buffer, sizeof(buffer));
	printf(buffer);
	printf("\n\n");

	fin.close();

	return true;
}

/********************/
// Console Commands //
/********************/



//pipe
//
// redirects output from str2 to str1, where str2 and str1 are system calls
// assumes str1 | str2
// Entirely based upon modifications of the example code provided
//
//str1 - the system command whose ouptput is piped into str2
//str2 - the system command to receive input from str1
void pipe(string str1, string str2)
{
  int fd_pipe[2];
  int pid1;
  int pid2;
  int status;
  int wpid;

  pid1 = fork(); //create child for input
  if (pid1 == 0)
    {

    pipe(fd_pipe);	// create pipe

    pid2 = fork();	//create grandchild for output
    if (pid2 == 0)
      {
      close(1);		// close standard output
      dup(fd_pipe[1]);	// redirect the output

      // close unnecessary file descriptors
      close(fd_pipe[0]);
      close(fd_pipe[1]);

      execl("/bin/sh", "sh", "-c", str1.c_str(), 0); //use shell to execute the output command
      cout << "execl of " + str1 + " failed.\n\n" << endl;
      return;
      }

    // back to process for input side of pipe

    close(0);              // close standard input
    dup(fd_pipe[0]);       // redirect the input

    // close unnecessary file descriptors
    close(fd_pipe[0]);     
    close(fd_pipe[1]);

    execl("/bin/sh", "sh", "-c", str2.c_str(), 0); //use shell to execute the input command
    cout << "execl of " + str2 + " failed.\n\n" << endl;
    return;
    }
  else
    {
    // parent process waits for children here
    rusage usage;
    int pid = wait3(&status, 0 , &usage);
    status = status >> 8;
    cout << "\n\nChild proccess " << pid << " exited with status " << status << endl;
    cout << "\nTime running in User Mode: " << usage.ru_utime.tv_sec << " seconds " << usage.ru_utime.tv_usec << " microseconds" << endl;
    cout << "Time running in System Mode: " << usage.ru_stime.tv_sec << " seconds " << usage.ru_stime.tv_usec << " microseconds" << endl;
    cout << "Number of Major Page Faults: " << usage.ru_majflt << endl;
    cout << "Number of Minor Page Faults: " << usage.ru_minflt << endl<<endl;
    }
}



//dclient
//
// starts a client, connecting to a server at a given IP and port
// receives a command, executes it and returns the output to the server
// Entirely based upon modifications of the example code provided
//
// command - whether or not to stop the server 
//	( for now, "stop" is to stop and "dclient" is to do what the server wants.)
// IP - the IP address of the server
// port - the port number of the server
void dclient(string command, string IP, int port)
{

int pid = fork();


if (pid == 0)
{
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr; 

    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n\n");
        return;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); 

    if(inet_pton(AF_INET, IP.c_str(), &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n\n");
        return;
    } 

    cout << "\nAttempting to connect...\n";
    cout.flush();


    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("Error : Connect Failed \n\n");
       return;
    } 

    cout << "\nConnection succesful.\n";


    cout << "\nAwaiting a command from the server...\n";
    cout.flush();
    while ( (n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0)
    {
	recvBuff[n] = 0; //null terminate
        pipe(string( recvBuff ), command);
        cout << "\nCommand received.\n";
    } 

    if(n < 0)
    {
	printf("\nRead error \n\n");
    }

    exit(0);

}

wait(NULL);

    return;
}

//dserve
//
// starts a server in the given port and sends
// a command to a client once one connects.  It then
// receives the client's response and prints it to the
// screen.
// Entirely based upon modifications of the example code provided
//
//command - the command to send to clients
//port - the port of the socket to open
void dserve(string command, int port)
{
	int pid = fork();

	if( pid == 0)
	{
	  int listenfd = 0, connfd = 0;
	  struct sockaddr_in serv_addr; 

	  char sendBuff[1025];
	  char recvBuff[1025];
	  time_t ticks; 

	  memset(sendBuff, '0', sizeof(sendBuff));

	  //put the command into the send buffer
	  for(int i = 0; i < command.length() && i < 1025; i++ )
	    sendBuff[i] = command[i];
	  if(command.length() < 1025)
	    sendBuff[command.length()] = '\0';
	  else
	    sendBuff[1024] = '\0';

	  listenfd = socket(AF_INET, SOCK_STREAM, 0);

	  memset(&serv_addr, '0', sizeof(serv_addr));
	
	  serv_addr.sin_family = AF_INET;
	  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	  serv_addr.sin_port = htons(port); 

	  bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 

	  listen(listenfd, 10); 

	  cout << "\nAttempting to accept connection...\n";
	  cout.flush();

	  connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
	
	  cout << "\nConnection accepted.\n";

	  ticks = time(NULL);

	  cout << "\nSending command to clients...\n";
	  cout.flush();

	  write(connfd, sendBuff, strlen(sendBuff));
	  cout << "\nCommand " << sendBuff << " sent.\n";

	  close(connfd);

	  exit(0);
	}

	wait(NULL);	

	return;
}



//redirect_left
//
// redirects from str2 to str1, where str2 is a file
// assumes str1 < str2, where str2 has no spaces.
// Entirely based upon modifications of the example code provided
//
//str1 - the system command to receive input from str2
//str2 - the file to be opened in read-only mode and redirected into str1
void redirect_left(string str1, string str2)
{
  int fpt1, pid, status;

  pid = fork();
  if (pid == 0)
    {
    // child process executes here

    if ((fpt1 = open(str2.c_str(), O_RDONLY)) == -1) //attempt to open the output on the right
    {
    	printf("Unable to open %s for reading.\n\n",str2.c_str());
        exit(-1);
    }
    close(0);       // close child standard input
    dup(fpt1);      // redirect the child input
    close(fpt1);    // close unnecessary file descriptor
    execl("/bin/sh", "sh", "-c", str1.c_str(), 0); //use shell to execute the input command on the left
    cout << "execl of " + str1 + " failed.\n\n" << endl;
    }
  else
  {
    /* parent process executes here */
    rusage usage;
    int pid = wait3(&status, 0 , &usage);
    status = status >> 8;
    cout << "\n\nChild proccess " << pid << " exited with status " << status << endl;
    cout << "Time running in User Mode: " << usage.ru_utime.tv_sec << " seconds " << usage.ru_utime.tv_usec << " microseconds" << endl;
    cout << "Time running in System Mode: " << usage.ru_stime.tv_sec << " seconds " << usage.ru_stime.tv_usec << " microseconds" << endl;
    cout << "Number of Major Page Faults: " << usage.ru_majflt << endl;
    cout << "Number of Minor Page Faults: " << usage.ru_minflt << endl<<endl;
    
  }
}


//redirect_right
//
// redirects from str1 to str2, where str2 is a file
// assumes str1 > str2, where str2 has no spaces.
// Entirely based upon modifications of the example code provided
//
//str1 - the system command to send output to str2
//str2 - the file to be opened in writing mode and written with the output from str2
void redirect_right(string str1, string str2)
{
  int fpt1, pid, status;

  pid = fork();
  if (pid == 0)
    {
    // child process executes here
    fpt1 = creat(str2.c_str(), 0644);
    if (fpt1 == -1) //attempt to open the output on the right
    {
    	printf("Unable to open %s for writing.\n\n",str2.c_str());
        exit(-1);
    }
    close(1);       // close child standard output
    dup(fpt1);      // redirect the child input
    close(fpt1);    // close unnecessary file descriptor
    
    execl("/bin/sh", "sh", "-c", str1.c_str(), 0); //use shell to execute the input command on the left
    cout << "execl of " + str1 + " failed.\n\n" << endl;
    }
  else
  {
    /* parent process executes here */
    rusage usage;
    int pid = wait3(&status, 0 , &usage);
    status = status >> 8;
    cout << "\n\nChild proccess " << pid << " exited with status " << status << endl;
    cout << "Time running in User Mode: " << usage.ru_utime.tv_sec << " seconds " << usage.ru_utime.tv_usec << " microseconds" << endl;
    cout << "Time running in System Mode: " << usage.ru_stime.tv_sec << " seconds " << usage.ru_stime.tv_usec << " microseconds" << endl;
    cout << "Number of Major Page Faults: " << usage.ru_majflt << endl;
    cout << "Number of Minor Page Faults: " << usage.ru_minflt << endl<<endl;
    
  }
}

//command_cmdnm
//
// displays the command string used to launch process number process id
//
//pid - the given process id
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_cmdnm(int pid, ostream& cout)
{
	ifstream fin;
	char buffer[1024];
	string command_string;
	string filepath = "/proc/" + to_string(pid) + "/cmdline";

	// Read the entire contents of the /prof file into the buffer
	fin.open(filepath, ios::in);

	if (!fin)
		return false;

	//read from the file
	fin.getline(buffer, sizeof buffer);
	
	command_string = string(buffer);

	cout << command_string << "\n\n";

	return true;
}

//command_signal
//
// sends a signal of type signal_num to process id pid
//
//signal_num - the given signal num to send
//pid - the given process id
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_signal(int signal_num, int pid, ostream& cout)
{
	int result = kill(pid, signal_num);
	if(result == -1)
		return false;
	else
		return true;
}

//commad_systat
//
// displays system statistics:
// Print out some process information 
// print (to stdout) Linux version information, and system uptime. 
// print memory usage information: memtotal and memfree at least. 
// print cpu information: vendor id through cache size. 
// using /proc/* files
//
//pid - the given process id
//cout - the ostream to display through
//
//returns - whether or not there was success
bool command_systat(ostream& cout)
{
	float idle_seconds = 0;
	float total_seconds = 0;

	if(!get_linuxinfo() || !get_cpuinfo(cout) || !get_uptime(cout, total_seconds, idle_seconds) || !get_meminfo())
		return false;

	//build and print out result strings for the uptime

	printf("UPTIME INFO:\n\n");
	printf(std::string("Total Uptime: " + std::to_string(total_seconds) + " Seconds \n").c_str());
	printf(std::string("Total Time Spent Idle: " + std::to_string(idle_seconds) + " Seconds \n").c_str());

	printf("\n");
	return true;
}



/********************/
// Main Input Loop  //
/********************/


//main
//
// Reads in user command through an input loop.  Will call various functions after evaluating the given command and parameters.
// Checks for misuse and errors in the functions, informing the user of successes, failures, and the mistyping of commands or parameters.
//
//returns - an error code. 0 is no errors.
int main ()
{
    int shm_sizes[128] = {-1};  //the sizes of each shared memory segment
    int shm_id[128] = {-1};     //the ids of each shared memory segment

    mexample_main();
    sexample_main();
    return 0;

	bool done = false;
	string input;
	string command;
	string args[2];
	char input_cstr[100];


	//register the signal handler
	signal (SIGINT, catch_signal);

	while( !done )
	{
		//check if the proccess was terminated
		if(exiting)
			return(0);

		cout << "dsh> ";
		cin.getline(input_cstr, 100);
		
		//check if the proccess was killed
		if(exiting)
			return(0);

		//clear the input stream
		cin.clear();
		
		input = string(input_cstr); //get the input
		int failed = -1; //the number of arguments which should be entered, 
				 //given a failed entering of a command.  If -1, the
				 //command was entered successfully.

		//first, check for a pipe
		int pipe_index = input.find("|");
		int redirect_left_index = input.find("<");
		int redirect_right_index = input.find(">");
		int remote_client_index = input.find("((");
		int remote_server_index = input.find("))");
		int split_index = -1;	

		if (pipe_index != -1)
		{
			split_index = input.find("|");
			if(split_index == 0)
				failed = 2;
			else if(split_index == (input.length()-1))
				failed = 2;
			else
			{
				string left = input.substr(0, split_index);
				string right = input.substr(split_index + 1);

				if(left.find("|") == -1 && right.find("|") == -1)
				{
					//sucess!  Begin the piping.
					pipe(left, right);
				}	
				else
					cout << "Only one pipe allowed in piping commands.\n\n";
			}
		}
		else if (redirect_left_index != -1)
		{
			split_index = input.find("<");
			if(split_index == 0)
				failed = 2;
			else if(split_index == (input.length()-1))
				failed = 2;
			else
				{
				string left = input.substr(0, split_index);
				string right = input.substr(split_index + 1);

				//remove spaces from the filename
				for(int i = 0; i < right.length(); i++)
					if(right[i] == ' ') right.erase(i,1);

				//sucess!  Begin the redirecting
				redirect_left(left, right);
				}
		}
		else if (redirect_right_index != -1)
		{
			split_index = input.find(">");
			if(split_index == 0)
				failed = 2;
			else if(split_index == (input.length()-1))
				failed = 2;
			else
				{
				string left_str = input.substr(0, split_index);
				string right = input.substr(split_index + 1);

				//remove spaces from the filename
				for(int i = 0; i < right.length(); i++)
					if(right[i] == ' ')
						right.erase(i,1);

				//sucess!  Begin the redirecting
				redirect_right(left_str, right);
				}
		}
		else if (remote_client_index != -1)
		{
			split_index = input.find("((");
			if(split_index == 0)
				failed = 3;
			else if(split_index == (input.length()-1))
				failed = 3;
			else
			{
				string left = input.substr(0, split_index);
				string right_str = input.substr(split_index + 2);

				//remove the spaces at the back of the first argument
				for(int i  = left.length()-1; i >= 0 && left[i] == ' '; i--)
					left.erase(i,1);

				//remove the spaces in front of the IP address
				for(int i  = 0; i < right_str.length() && right_str[i] == ' '; i++)
					right_str.erase(i,1);

				split_index = right_str.find(" ");
				if(split_index == 0)
					failed = 3;
				else if(split_index == (right_str.length()-1))
					failed = 3;
				else
				{
					string IP = right_str.substr(0, split_index);
					string port_str = right_str.substr(split_index + 1);

					//make sure that the port is an int
					if (is_integer(port_str))
					{
						int port = atoi(port_str.c_str());
										
						//sucess!  Begin the client
						dclient(left, IP, port);
					}
					else
						cout << "\nPort must an integer.\n\n";
				}
			}
		}
		else if (remote_server_index != -1)
		{
			//remove spaces from the input
			for(int i = 0; i < input.length(); i++)
			{
				if(input[i] == ' ') input.erase(i,1);
			}
			split_index = input.find("))");
			if(split_index == 0)
				failed = 2;
			else if(split_index == (input.length()-1))
				failed = 2;
			else
			{
				string left = input.substr(0, split_index);
				string right = input.substr(split_index + 2);
				int port = -1;
			
				if(is_integer(right))
				{

					port = atoi(right.c_str());
					//sucess!  Begin the server
					dserve(left, port);
				}
				else
					cout << "\nPort must be an integer.\n\n";
			}
		}
		else //no piping or redirecting
		{
			split_index = input.find(" ");
			command = input.substr(0,split_index);

			if(command == "cmdnm")
			{
				if(input.find(" ") == -1)
					failed = 1;
				else
				{
					args[0] = input.substr(split_index + 1);

					//check to see if the argument was an integer
					if(!is_integer(args[0]))
					{
						cout << "Argument pid must be an integer.\n\n";
					}
					//check to see if there was only one argument	
					if(args[0].find(" ") == -1)
					{
						if (!command_cmdnm(atoi(args[0].c_str()), cout))
							cout << "Failed to retrive command string for proccess " + args[0] + ".\n\n";
					}
					else
						failed = 1;
					}
			}
			else if(command == "signal")
			{

				if(input.find(" ") == -1)
					failed = 2;
				else
					{

					string arguments = input.substr(split_index + 1);
					args[0] = arguments.substr(0, arguments.find(" "));
					args[1] = arguments.substr(arguments.find(" ") + 1);

					//check to see if the arguments were both integers
					if(!is_integer(args[0]) || !is_integer(args[1]))
					{
						cout << "Arguments signal_num and pid must be integers.\n\n";
					}
					//check to see if there were more than two arguments or only one argument
					else if(args[1].find(" ") == -1 && arguments.find(" ") != -1)
					{
						if (!command_signal(atoi(args[0].c_str()), atoi(args[1].c_str()), cout))
							cout << "Failed to send singal " + args[0] + " to proccess " + args[1] + ".\n\n";
					}
					else
						failed = 2;
				}
			}
            else if(command == "mboxinit")
            {
            if(input.find(" ") == -1)
					failed = 2;
				else
					{

					string arguments = input.substr(split_index + 1);
					args[0] = arguments.substr(0, arguments.find(" "));
					args[1] = arguments.substr(arguments.find(" ") + 1);

					//check to see if the arguments were both integers
					if(!is_integer(args[0]) || !is_integer(args[1]))
					{
						cout << "Arguments num_mailboxes and mailbox_size must be integers.\n\n";
					}
					//check to see if there were more than two arguments or only one argument
					else if(args[1].find(" ") == -1 && arguments.find(" ") != -1)
					{
						if (!command_mboxinit(shm_sizes, shm_id, atoi(args[0].c_str()), atoi(args[1].c_str()), cout))
							cout << "Failed to init " + args[0] + " mailboxes with size " + args[1] + "kb.\n\n";
					}
					else
						failed = 2;
				}
            }
            else if(command == "mboxdel")
            {
            if(input.find(" ") != -1)
					failed = 0;
				else
				{
					if (!command_mboxdel(cout))
						cout << "Failed to delete mailboxes and semaphores.\n\n";
				}
            }
            else if(command == "mboxwrite")
            {
            if(input.find(" ") == -1)
					failed = 1;
				else
				{
					args[0] = input.substr(split_index + 1);

					//check to see if the argument was an integer
					if(!is_integer(args[0]))
					{
						cout << "Argument mailbox must be an integer.\n\n";
					}
					//check to see if there was only one argument	
					if(args[0].find(" ") == -1)
					{
						if (!command_mboxwrite(shm_sizes, shm_id, atoi(args[0].c_str()), cout))
							cout << "Failed to open mailbox " + args[0] + " for writing.\n\n";
					}
					else
						failed = 1;
					}
            }
            else if(command == "mboxread")
            {
            if(input.find(" ") == -1)
					failed = 1;
				else
				{
					args[0] = input.substr(split_index + 1);

					//check to see if the argument was an integer
					if(!is_integer(args[0]))
					{
						cout << "Argument mailbox must be an integer.\n\n";
					}
					//check to see if there was only one argument	
					if(args[0].find(" ") == -1)
					{
						if (!command_mboxread(shm_sizes, shm_id, atoi(args[0].c_str()), cout))
							cout << "Failed to open mailbox " + args[0] + " for reading.\n\n";
					}
					else
						failed = 1;
					}
            }
            else if(command == "mboxcopy")
            {
            if(input.find(" ") == -1)
					failed = 2;
				else
					{

					string arguments = input.substr(split_index + 1);
					args[0] = arguments.substr(0, arguments.find(" "));
					args[1] = arguments.substr(arguments.find(" ") + 1);

					//check to see if the arguments were both integers
					if(!is_integer(args[0]) || !is_integer(args[1]))
					{
						cout << "Arguments boxnumber1 and boxnumber2 must be integers.\n\n";
					}
					//check to see if there were more than two arguments or only one argument
					else if(args[1].find(" ") == -1 && arguments.find(" ") != -1)
					{
						if (!command_mboxcopy(shm_sizes, shm_id, atoi(args[0].c_str()), atoi(args[1].c_str()), cout))
							cout << "Failed to copy data from mailbox " + args[0] + " to mailbox " + args[1] + ".\n\n";
					}
					else
						failed = 2;
				}
            }
			else if(command == "cd")
			{

				if(input.find(" ") == -1)
					failed = 1;
				else
				{
					args[0] = input.substr(split_index + 1);

					//check to see if the argument was an integer
					if(chdir(args[0].c_str()) == -1)
					{
						cout << "Failed to change working directory to " << args[0] << ".\n\n";
					}
					else
						cout << "Working directory changed to " << getcwd(0, 0) << "\n\n";
				}
			}
			else if(command == "systat")
				if(input.find(" ") != -1)
					failed = 0;
				else
				{
					if (!command_systat(cout))
						cout << "Failed to retrieve system statistics.\n\n";
				}
			else if(command == "exit")
				//systat logic here
				if(input.find(" ") != -1)
					failed = 0;
				else
				{
					done = true;
				}
			else
			{
				if(command != "")
				{
					int pid = fork();
					if (pid == 0)
					{
					    // child process executes here
					    execl("/bin/sh", "sh", "-c", input.c_str(), 0); //use shell to execute the input command on the left
					    cout << "execl of " + command + " failed.\n\n" << endl;
					}
					int status;
					rusage usage;
					pid = wait3(&status, 0 , &usage);
					status = status >> 8;
					cout << "\n\nChild proccess " << pid << " exited with status " << status << endl;
					cout << "\nTime running in User Mode: " << usage.ru_utime.tv_sec << " seconds " << usage.ru_utime.tv_usec << " microseconds" << endl;
					cout << "Time running in System Mode: " << usage.ru_stime.tv_sec << " seconds " << usage.ru_stime.tv_usec << " microseconds" << endl;
					cout << "Number of Major Page Faults: " << usage.ru_majflt << endl;
					cout << "Number of Minor Page Faults: " << usage.ru_minflt << endl<<endl;
				}
			}
		}

		//check to see if there was an argument number-based a failure.  If there was, let the user know.
		if(failed != -1)
		{
			cout << command + " takes " + std::to_string(failed) + " argument" + (failed == 1 ? "" : "s") + ".\n\n";
		}
	}
	cout << "Exiting application.\n\n";
	return 0;
}
