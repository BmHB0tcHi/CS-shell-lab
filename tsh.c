/* \
 * tsh - A tiny shell program with job control
 *
 *
 <
 
 	--> Name: Bashar Abuhassan
 	--> System programming shell-lab
 >
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    100   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);


/* Here are the functions that you will implement */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
        break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
        break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
        break;
    default:
            usage();
    	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

    /* Read command line */
    if (emit_prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
        app_error("fgets error");
    if (feof(stdin)) { /* End of file (ctrl-d) */
        fflush(stdout);
        exit(0);
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    // printf("Entered eval..\n");
    char *args[MAXARGS];  
    int isBG;   // if JOb is requested to be in background       
    sigset_t mask;

    // parsing the line
    isBG = parseline(cmdline, args);

    // lets see if our args are built it in not
    // if not, then we fork and execute the args as normal.
    if(!builtin_cmd(args)) { 
        
        // lets block first
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        // forking
        pid_t jobpid = fork();
        // checking for errors while forking
        if(jobpid < 0){
           printf("Error while forking...\n");
           exit(1);
        }
        // child process
        else if(jobpid == 0) {

            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            setpgid(0, 0);
            //executing the command, while also checking for IF the command is there
            if(execvp(args[0], args) < 0) {
                printf("%s: Command not found\n", args[0]);
                exit(1);
            }
        } 
        // parent proccess
        else {
            if(!isBG){
                // add a job in FG state
                addjob(jobs, jobpid, FG, cmdline);
            }
            else {
                // add job in BG[Background state]
                addjob(jobs, jobpid, BG, cmdline);
            }
            sigprocmask(SIG_UNBLOCK, &mask, NULL);

            if (!isBG){
                // wait if the job is in FG
                waitfg(jobpid);
            } 
            else {
                //printing attributes for the job
                printf("[%d] (%d) %s", pid2jid(jobpid), jobpid, cmdline);
            }
        }
    }
    //printf("Leaving Eval...")
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    // printf("Entering: parseline\n");
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    }
    else {
        delim = strchr(buf, ' ');
    }

    while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        }
        else {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
    return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
        argv[--argc] = NULL;
    }
    return bg;
    // printf("leaving Parseline\n");
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    // printf("Entering builtin_cmd")
    if(!argv[1]){
        int return_val = strcmp(argv[0],"jobs");
        if (!return_val) {
            //printf("You Requested Jobs\n");
            listjobs(jobs);
            return 1;
        }
        if(!strcmp(argv[0],"quit")) {
            puts("Exiting..");
            exit(1);
        }
        else if (!strcmp("&", argv[0])) {
            return 1;
        }
    
    }
    // if the first arguement is fg or bg, then we exec the do_bgfg command
    else if (!strcmp("bg", argv[0]) || !(strcmp("fg", argv[0]))) {  
        //call bgfg
        do_bgfg(argv);  
        return 1;  
    }
    // printf("Leaving builtin_cmd");
    return 0;
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{   
    // printf("Entering do_bgfg\n");
    struct job_t *jobptr;
    char *second_arg;
    int jobid;
    pid_t jobpid;

    second_arg = argv[1];
    
    // iif second_arg[WHich is either a pid or jid] doesnt exist
    if(second_arg == NULL) {
        puts("Second Arguement needed..");
        return;
    }
    
    // checking if first character of secondarg is representing a Job ID[jid]
    if(second_arg[0] == '%') {  
        jobid = atoi(&second_arg[1]); 
        //get a pointer to job requested by second arguement
        jobptr = getjobjid(jobs, jobid);
        // if jobptr doesnt point to something
        if(jobptr == NULL){  
            printf("%s: No such job\n", second_arg);  
            return;  
        }else{
            jobpid = jobptr->pid;
        }
    } 
    // Checking if Second arg starts with a digit, this means its a PID of some job
    else if(isdigit(second_arg[0])) { 
        //get jobpid
        jobpid = atoi(second_arg); 
        jobptr = getjobpid(jobs, jobpid); 
        if(jobptr == NULL){  
            printf("(%d): No such process\n", jobpid);  
            return;  
        }  
    }  
    else {
        puts("Second arguement needed[PID or JID]");
        return;
    }
    // sending signal
    kill(-jobpid, SIGCONT);
    
    if(!strcmp("fg", argv[0])) {
        //wait for fg proccess
        jobptr->state = FG;
        waitfg(jobptr->pid);
    } 
    else{
        //print attributes of job
        printf("[%d] (%d) %s", jobptr->jid, jobptr->pid, jobptr->cmdline);
        jobptr->state = BG;
    } 
    // printf("Leaving do_bgfg\n");
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t jobpid)
{
    // printf("Entering waitfg\n");
    struct job_t* jobptr;
    // retrieving a pointer to datatype of struct job_t
    jobptr = getjobpid(jobs,jobpid);
    //if pid is valid
    if(jobpid == 0) {
        return;
    }
    if(jobptr != NULL) {
        //we wait until jobpid is not in the foreground group
        while(jobpid==fgpid(jobs)){
        }
    }
    return;
    // printf("Leaving watifg\n");
}

/*****************
 * Signal handlers
 *****************/
/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    pid_t jobpid;  
    int Stats;
    /*  SELF NOTES
        waitpid systemcall require 3 args
        first arg is Proccess ID
        second arg is The second argument is of type int pointer and we should declare an,
        integer variable to pass its address to the function.
        the last argument is of type int, and it’s used to specify the certain child process events to monitor in addition to default ones.
    
        We just want to collect the status of a dead process if there are any. That's what WNOHANG is for.
        WUNTRACED allows the parent to be returned from wait()/waitpid() if a child gets stopped as well as exiting or being killed. This way,
        the  parent has a chance to send it a SIGCONT to continue it, kill it, assign its tasks to another child, whatever.
    */
    while ((jobpid = waitpid(fgpid(jobs), &Stats, WNOHANG|WUNTRACED)) > 0) {  
        if (WIFSTOPPED(Stats)){ 
            //change state if job is stopped
            getjobpid(jobs, jobpid)->state = ST;
            int jobid = pid2jid(jobpid);
            // printing feedback to user
            printf("Job [%d] (%d) Stopped by signal %d\n", jobid, jobpid, WSTOPSIG(Stats));
        } 
        else if (WIFSIGNALED(Stats)){
            int jid = pid2jid(jobpid);  
            printf("Job [%d] (%d) terminated by signal %d\n", jid, jobpid, WTERMSIG(Stats));
            // lets delete the job
            deletejob(jobs, jobpid);
        } 
        else if (WIFEXITED(Stats)){  
            //exited
            deletejob(jobs, jobpid);  
        }  
    }  
    return; 
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    pid_t jobpid = fgpid(jobs);  
    if (jobpid != 0) {     
        kill(-jobpid, sig);
    }   
    return;   
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    pid_t jobpid = fgpid(jobs);  
    if (jobpid != 0) { 
        kill(-jobpid, sig);  
    }  
    return;   
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;
    for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    if (pid < 1)
    return 0;

    for (i = 0; i < MAXJOBS; i++) {
        // Empty Place is represented with jobpid = 0
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            // incrementing global nextjid variable
            jobs[i].jid = nextjid++;

            if (nextjid > MAXJOBS)
            nextjid = 1;

            strcpy(jobs[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
                return 1;
        }   
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;
    if (pid < 1)
    return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;
    for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
        return jobs[i].pid;
    return 0;
}

/* 
    getjobpid  - Find a job (by PID) on the job list 
    return Null if job not found 
*/ 
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;
    if (pid < 1)
    return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* 
    getjobjid  - Find a job (by JID) on the job list 
    Return Null pointer if job not found
*/
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;
    if (jid < 1)
    return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    
    return NULL;
}

/* 
    pid2jid - Map process ID to job ID 
    return 0 if job with requested pid not found
*/

int pid2jid(pid_t pid) 
{
    int i;
    if (pid < 1)
    return 0;
    for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* 
    listjobs - Print the job list 
     Example : JID PID STATE CMDLINE
*/
void listjobs(struct job_t *jobs) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state){
            case BG: 
                printf("Running ");
                    break;
            case FG: 
                printf("Foreground ");
                break;
            case ST: 
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ", 
                        i, jobs[i].state);
            }
        printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) {
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg){
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg){
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
