#include <stdio.h>
#include <stdlib.h>
#include<sys/types.h>
#include <sys/wait.h>
#include<dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#define MAXCHAR 1000
#define MAXJOB 16
#define STATUS_RUNNING 0
#define STATUS_DONE 1
#define STATUS_SUSPENDED 2
#define STATUS_CONTINUED 3
#define STATUS_TERMINATED 4
#define PROC_FILTER_ALL 0
#define PROC_FILTER_DONE 1
#define PROC_FILTER_LEFT 2
#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024
#define TOKEN_BUFSIZE 64
pid_t foreground_pid;
int jobID = 1;
int count =0;
int proc_count = 0;
char *workingDir;
void handle_sigtstp();
void handle_sigcont(int);
void handle_sigint();
void _ls();
void ls();
void print_jobs();
void addJobs( pid_t, int, char[]);
void built_In_Function(char[]);
void pwd();
void bg(int , char**);
void fg(int , char**);
void cd(char[]);
void launch_jobs(char[], char**);
void init_process(pid_t, int, char, char);
int setJstatus(int, int); 
int setPstatus(int, int);
void init_jobs(pid_t, int, char**);
int getPgid(int);
int printJstatus(int);
int waitJob(int);
int waitForPid(int);
int removeJob(int);
void killing(int, char**);
int getProcCount(int, int);
int endJob(int);

//STRUCTS
struct process 
{
    char *command;
    int argc;
    char **argv;
    char *input_path;
    char *output_path;
    pid_t pid;
    int type;
    int status;
    struct process *next;
};

struct Jobs{
	pid_t gpid;
	int jobID;
	int state; 
	char cmd[100];
};
struct Jobs jobs[10];

struct Process{
	pid_t pid; 
	int status; 
	char complete; 
	char stop;
};
struct Process proc[100];

struct job 
{
    int id;
    struct process *root;
    char *command;
    pid_t pgid;
    int mode;
};

struct job *getJob(int);

struct shellInfo {
    char cur_user[TOKEN_BUFSIZE];
    char cur_dir[PATH_BUFSIZE];
    char pw_dir[PATH_BUFSIZE];
    struct job *jobs[MAXJOB + 1];
};

struct shellInfo *shell;

const char* STATUS_STRING[] = {"running", "done", "suspended", "continued", "terminated"};

//PROCESSES
void init_process(pid_t pid, int status, char c, char s){
	if(proc_count > 100){
		printf("Process list is full.\n");
		return;
	}
	proc[proc_count].pid = pid;
	proc[proc_count].status = status;
	proc[proc_count].complete = c;
	proc[proc_count].stop = s;
	proc_count++;

}

int printJstatus(int id) 
{
    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) 
    {
        printf("\t%d\t%s\t%s", proc->pid,STATUS_STRING[proc->status], proc->command);
        if (proc->next != NULL) 
        {
            printf("|\n");
        } 
        
        else 
        {
            printf("\n");
        }
    }
    return 0;
}

struct job *getJob(int id) 
{
    return shell->jobs[id];
}

int endJob(int id) 
{
    struct job *job = shell->jobs[id];
    struct process *proc, *tmp;
    for (proc = job->root; proc != NULL; ) {
        tmp = proc->next;
        free(proc->command);
        free(proc->argv);
        free(proc->input_path);
        free(proc->output_path);
        free(proc);
        proc = tmp;
    }

    free(job->command);
    free(job);

    return 0;
}

int getProcCount(int id, int filter) 
{
    int count = 0;
    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) 
    {
        if (filter == PROC_FILTER_ALL ||(filter == PROC_FILTER_DONE && proc->status == STATUS_DONE) ||
            (filter == PROC_FILTER_LEFT && proc->status != STATUS_DONE)) 
        {
            count++;
        }
    }
    return count;
}

void init_jobs(pid_t gpid, int state, char **command){
	if(count >10){
		printf("Job list is full!\n");
		return;
	}
	jobs[count].gpid = gpid;
	jobs[count].state = state;
	jobs[count].jobID = jobID;
	jobID++;
    count++;
	strcpy(jobs[count].cmd,command[0]);
}

char **tokenize(char *string, char **argv, int argc)
{
	int i = 0;
	char *temp = strtok(string, " ");
	while (temp != NULL)
	{
		argv[i] = temp;
		temp = strtok(NULL, " ");
		i++;
	}
	return argv;
}

int setJstatus(int id, int status) 
{
    
    struct process *proc;

    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) 
    {
        if (proc->status != STATUS_DONE) 
        {
            proc->status = status;
        }
    }
    return 0;
}

void fg(int argc, char **argv) 
{
    if (argc < 2) 
    {
        printf("fg \n");
        return;
    }

    //int status;
    pid_t pid;
    int jobId = -1;

    if (argv[1][0] == '%') 
    {
        jobId = atoi(argv[1] + 1);
        pid = getPgid(jobId);
        if (pid < 0) 
        {
            printf("%s: no such job found\n", argv[1]);
            return;
        }
    } 
    
    else 
    {
        pid = atoi(argv[1]);
    }

    tcsetpgrp(0, pid);

    if (jobId > 0) 
    {
        setJstatus(jobId, STATUS_CONTINUED);
        printJstatus(jobId);
        if (waitJob(jobId) >= 0) 
        {
            removeJob(jobId);
        }
    } 
    
    else 
    {
        waitForPid(pid);
    }

    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(0, getpid());
    signal(SIGTTOU, SIG_DFL);
}



int waitJob(int id) 
{
    int proc_count = getProcCount(id, PROC_FILTER_LEFT);
    int wait_pid = -1;
    int wait_count = 0;
    int status = 0;
    do {
        wait_pid = waitpid(-shell->jobs[id]->pgid, &status, WUNTRACED);
        wait_count++;

        if (WIFEXITED(status)) 
        {
            setPstatus(wait_pid, STATUS_DONE);
        } 
        
        else if (WIFSIGNALED(status)) 
        {
            setPstatus(wait_pid, STATUS_TERMINATED);
        } 
        
        else if (WSTOPSIG(status)) 
        {
            status = -1;
            setPstatus(wait_pid, STATUS_SUSPENDED);
            
            if (wait_count == proc_count) 
            {
                printJstatus(id);
            }
        }
    } while (wait_count < proc_count);

    return status;
}

int setPstatus(int pid, int status) 
{
    int i;
    struct process *proc;

    for (i = 1; i <= MAXJOB; i++) 
    {
        if (shell->jobs[i] == NULL) 
        {
            continue;
        }
        
        for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) 
        {
            if (proc->pid == pid) 
            {
                proc->status = status;
                return 0;
            }
        }
    }
    return -1;
}

int removeJob(int id) 
{
    endJob(id);
    shell->jobs[id] = NULL;

    return 0;
}

int waitForPid(int pid) 
{
    int status = 0;

    waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) 
    {
        setPstatus(pid, STATUS_DONE);
    } 
    
    else if (WIFSIGNALED(status)) 
    {
        setPstatus(pid, STATUS_TERMINATED);
    } 
    
    else if (WSTOPSIG(status)) 
    {
        status = -1;
        setPstatus(pid, STATUS_SUSPENDED);
    }
    count--;
    return status;
}

int getPgid(int id) 
{
    struct job *job = getJob(id);

    return job->pgid;
}

void cd(char *path){
	char cwd[100];
	int ret = chdir(path);
	if(ret ==0){
		getcwd(cwd, 100);
		printf("%s\n", cwd);
	}else perror("Could not change dir");
	
}

void bg(int argc, char **argv) 
{
    if (argc < 2) 
    {
        printf("bg \n");
        return;
    }

    pid_t pid;
    int jobId = -1;

    if (argv[1][0] == '%') 
    {
        jobId = atoi(argv[1] + 1);
        pid = getPgid(jobId);
        if (pid < 0) {
            printf("%s: no such job found\n", argv[1]);
            return;
        }
    } 
    
    else 
    {
        pid = atoi(argv[1]);
    }

    if (jobId > 0) 
    {
        setJstatus(jobId, STATUS_CONTINUED);
        printJstatus(jobId);
    }
}

void killing(int argc, char **argv) 
{
    if (argc < 3) 
    {
        printf("kill \n");
        return;
    }

    pid_t pid;
    int jobId = -1;

    if (argv[1][0] == '%') 
    {
        jobId = atoi(argv[2]);
        pid = getPgid(jobId);
        if (pid < 0) 
        {
            printf("%s: no such job found\n", argv[1]);
            return;
        }
        pid = -pid;
    } 
    else 
    {
        pid = atoi(argv[1]);
    }

    if (jobId > 0) 
    {
        setJstatus(jobId, STATUS_TERMINATED);
        printJstatus(jobId);
        if (waitJob(jobId) >= 0) 
        {
            removeJob(jobId);
        }
        count--;
    }

    return;
}

void pwd(){
	char cwd[100];
	getcwd(cwd, 100);
	printf("%s\n", cwd);

}

//LAUNCH JOBS
void launch_jobs(char *cmd, char  **command){	
	pid_t pid; 
	int status;
	pid = fork();
	foreground_pid = getpgrp();
	if(pid == 0){
		execv(cmd, command);
	}
	else{
		waitpid(pid, &status, WNOHANG);
		init_jobs(pid, status, command);
        init_process(pid, status, 'c', 's');
	}

	fflush(stderr);
	

}

void print_jobs(){
	for(int i = 0; i < count; i++ ){

		if(jobs[i].state <0)
			printf("[%d]    %d  %s  %s\n", jobs[i].jobID, jobs[i].gpid, "Terminated", jobs[i].cmd );
		else
			printf("[%d]    %d  %s  %s\n", jobs[i].jobID, jobs[i].gpid, "Running", jobs[i].cmd );

	}

}


//HANDLERS
void handle_sigtstp(){
	printf("\n");
    printf("> ");
}

void handle_sigcont(int sig){
	  if(sig != SIGCONT) {
        return;
    }   
	fflush(stdout);
}

void handle_sigint(){
	int ret;
	ret = kill(foreground_pid, SIGTERM);
	if(ret == -1){
		perror("kill");
		exit(EXIT_FAILURE);
	}
	else
		exit(EXIT_SUCCESS);

	fflush(stdout);
}

void handle_sigchld(int sig){
	
}

//BUILT IN COMMANDS
void commands(char **command, int argc)
{
	
	if (command[0][0] == '.')
	{
		launch_jobs(command[0],command);
	}

	else if (strcmp(command[0], "bg") == 0)
	{
		if(argc <2){
			return;
		}
		bg(argc, command);
	}

	else if (strcmp(command[0], "cd") == 0)
	{
		cd(command[1]);
	}

	else if (strcmp(command[0], "fg") == 0)
	{
		if(argc < 2){
			return;
		}
		fg(argc, command);
	}

    else if(strcmp(command[0], "job") == 0){
        if(argc < 2){
            printf("Job ID required.\n");
            return;
        }
        else
            printJstatus(command[1][0]);
    }
	else if (strcmp(command[0], "jobs") == 0)
	{
		print_jobs();
	}

	else if (strcmp(command[0], "kill") == 0)
	{
        killing(argc, command);
	}
	else if(strcmp(command[0], "clear") == 0)
	{
		system("clear");
	}
	else if(strcmp(command[0], "pwd") ==0)
	{
		pwd();
	}

	else if(strcmp(command[0], "ls") == 0)
	{
		if(argc <=1){
			_ls();
		}
		else if(strcmp(command[1], "-l") ==0){
			ls();
		}
	}

	else printf("%s: command not found\n", command[0]);
}

int getArgC(char *string)
{
	int argc = 0;
	char *temp = strtok(string, " ");
	while (temp != NULL)
	{
		++argc;
		temp = strtok(NULL, " ");
	}
	return argc;
}


//START OF LS
void _ls()
{
	struct dirent *d;
	DIR *dh = opendir(".");

	while((d = readdir(dh)) != NULL)
	{
		if(d->d_name[0] =='.')
			continue;
		printf("%s\n", d->d_name);
	}
	if(closedir(dh) < 0)
		printf("Error closing directory");
}
//IF USER DOES -l
void ls(){
struct dirent *d;
struct stat buf;
struct passwd *pws;
struct group *gru;


   DIR *dh = opendir(".");

   while((d = readdir(dh)) != NULL)
   {
	if(errno){
        	printf("error!");
	}

	char *path = realpath(d->d_name, NULL);
	stat(path, &buf);
	free(path);
	if(d->d_name[0] == '.')
		continue;


	//Getting attributes of stat to print
	int size = buf.st_size;
	uid_t uid = buf.st_uid;
	uid_t gid = buf.st_gid;
	gru = getgrgid(gid);
	pws = getpwuid(uid);
	char permissions[50];
	char time[30];
	strftime(time, 30, "%b %d %H:%M", gmtime(&buf.st_ctime));

	//setting permissions
	permissions[0] = (S_ISDIR(buf.st_mode))? 'd' : '-';
	permissions[1] = (buf.st_mode & S_IRUSR)? 'r' : '-';
	permissions[2] = (buf.st_mode & S_IWUSR)? 'w' : '-';
	permissions[3] = (buf.st_mode & S_IXUSR)? 'x' : '-';
	permissions[4] = (buf.st_mode & S_IRGRP)? 'r' : '-';
	permissions[5] = (buf.st_mode & S_IWGRP)? 'w' : '-';
	permissions[6] = (buf.st_mode & S_IXGRP)? 'x' : '-';
	permissions[7] = (buf.st_mode & S_IROTH)? 'r' : '-';
	permissions[8] = (buf.st_mode & S_IROTH)? 'w' : '-';
	permissions[9] = (buf.st_mode & S_IROTH)? 'x' : '-';
	printf("%s", permissions);

	//permissions, user id, group id, size, time accessed, filename
	printf(" %s %s %d %s %s\n", pws->pw_name , gru->gr_name, size, time, d->d_name);

}
	if(d == NULL){
		if(closedir(dh) < 0){
			printf("Error Closing File");
		}

		
	}


}//END OF LS 
int main(){
	signal(SIGTSTP, &handle_sigint);
	signal(SIGINT, &handle_sigtstp);
	signal(SIGCONT, &handle_sigcont);
	signal(SIGCHLD, &handle_sigchld);

	char userInput[1024];
	printf("> ");
	while(scanf("%[^\n]%*c", userInput) != EOF)
	{
			
		if(strcmp(userInput, "exit") == 0){
			break;
		}

		if (strlen(userInput) > 0)
		{
			char inputc[100];
			strcpy(inputc, userInput);
			int argc = getArgC(userInput);

			char **argv = (char**)malloc(argc * sizeof(char*));
			for (int i = 0; i<argc; i++)
				argv[i] = (char*)malloc(TOKEN_BUFSIZE);

			tokenize(inputc, argv, argc);
            
			commands(argv, argc);
			free(argv);
		}

		printf("> ");
	}
	//exit_shell();
}

