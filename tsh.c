
/* 
 * tsh - A tiny shell program with job control
 * <The line above is not a sufficient documentation.
 *  You will need to write your program documentation.>
 */

#include "tsh_helper.h"
#include <stdlib.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

#define MAXJOBS         16 
bool fgjobrunning = true;

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);

void sigterm_handler(int sig);
void sigkill_handler(int sig);
void sigusr1handler(int sig);

int pidparsebgfg(struct cmdline_tokens );
/*
 * <Write main's function header documentation. What does main do?>
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE_TSH];  // Cmdline for fgets
    bool emit_prompt = true;    // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    Dup2(STDOUT_FILENO, STDERR_FILENO);

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h':                   // Prints help message
            usage();
            break;
        case 'v':                   // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p':                   // Disables prompt printing
            emit_prompt = false;  
            break;
        default:
            usage();
        }
    }

    // Install the signal handlers
    signal(SIGINT,  sigint_handler);   // Handles ctrl-c
    Signal(SIGTSTP, sigtstp_handler);  // Handles ctrl-z
    signal(SIGCHLD, sigchld_handler);  // Handles terminated or stopped child
    signal(SIGKILL, sigkill_handler);
    
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler); 
    Signal(SIGTERM, sigterm_handler);
    Signal(SIGUSR1, sigusr1handler);
    
    // Initialize the job list
    initjobs(job_list);

    // Execute the shell's read/eval loop
    while (true)
    {
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin))
        {
            app_error("fgets error");
        }

        if (feof(stdin))
        { 
            // End of file (ctrl-d)
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }
        
        // Remove the trailing newline
        cmdline[strlen(cmdline)-1] = '\0';
        
        // Evaluate the command line
        eval(cmdline);
        fflush(stdout);
    } 
    
    return -1; // control never reaches here
}


/* Handy guide for eval:
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg),
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.
 * Note: each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */

/* 
 * <What does eval do?>
 */
void eval(const char *cmdline) 
{
    sigset_t mask;
    int status;
    parseline_return parse_result;     
    struct cmdline_tokens token;

    parse_result = parseline(cmdline, &token);

    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY)
    {
        return;
    }
    switch(token.builtin) {

       case BUILTIN_NONE  :
            if(parse_result == PARSELINE_FG){ //run in foreground
                  ;
                        //MASK the signals
                        
                        sigemptyset(&mask);
                        sigfillset(&mask);
                        sigprocmask(SIG_BLOCK, &mask, NULL);
                
                  int fgprocessid = Fork();

                  if(fgprocessid == 0){//child process

                    Setpgid(0,0);
                    Signal(SIGINT, SIG_DFL);
                    Signal(SIGCHLD, SIG_DFL);
                    Signal(SIGTSTP, SIG_DFL);
                    Sigprocmask(SIG_UNBLOCK, &mask, NULL);

                    execve(token.argv[0], token.argv, environ);
                  }
                  else if (fgprocessid > 0)//parent process
                  {                        
                      addjob(job_list, fgprocessid, FG, cmdline);
                      sigprocmask(SIG_UNBLOCK, &mask, NULL); //can only get sigchld sigint sigstp now
                        
                      sigset_t fullmask;
                      sigemptyset(&fullmask);
                      sigfillset(&fullmask);
                      sigdelset(&fullmask, 10);
                      sigdelset(&fullmask, 17);
                      sigdelset(&fullmask, 2);
                      
                      //printf("Before suspending parent\n");
                      
                      while(fgjobrunning){
                          sigsuspend(&fullmask);
                      }
                      fgjobrunning = true;
              
                      //printf("Parent block released after sigsuspend \n");
                      /*
                      if(pid == -1){//child reaped in handler. deleted from job list. so do nothing
                          break;
                      }
                      
                      if(WIFSTOPPED(status) !=0){ //if child was stopped 
                          sigemptyset(&mask);
                          sigfillset(&mask);
                          sigprocmask(SIG_BLOCK, &mask, NULL);
                          printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(job_list, pid), pid, 20);
                          //change the state of child in job_list

                          struct job_t* curr_job = getjobpid(job_list, fgprocessid);
                          curr_job->state = ST;
                          sigprocmask(SIG_UNBLOCK, &mask, NULL);
                          break;
                      }
                      
                          sigemptyset(&mask);
                          sigfillset(&mask);
                          sigprocmask(SIG_BLOCK, &mask, NULL);
                      if(WIFSIGNALED(status) != 0){//gets called when child dies of signal like SIGINT and parent reaps it not handler
                            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(job_list, pid), pid, WTERMSIG(status));
                       }

                      struct job_t *job = getjobpid(job_list, fgprocessid);
                      if(job != NULL){
                          deletejob(job_list, fgprocessid);  
                      }
                      sigprocmask(SIG_UNBLOCK, &mask, NULL);
                      */
                      break; 
                  }

              break;  
            }
            else{//run in background
                    
                    sigemptyset(&mask);
                    sigfillset(&mask);
                    sigprocmask(SIG_BLOCK, &mask, NULL);
                
                  int bgprocessid = Fork();

                  if(bgprocessid == 0){//child process
                    Setpgid(0,0);
                    Signal(SIGINT, SIG_DFL);
                    Signal(SIGCHLD, SIG_DFL);
                    Signal(SIGTSTP, SIG_DFL);
                    sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    execve(token.argv[0], token.argv, environ);
                  }
                  else
                  {                 
                    addjob(job_list, bgprocessid, BG, cmdline);
                           
                    int job_id = pid2jid(job_list, bgprocessid);
                     sigprocmask(SIG_UNBLOCK, &mask, NULL);
                      
                    //char * argumentt = token.argv[0] ;
                    printf("[%d] (%d) ", job_id, bgprocessid);
                    int i = 0;
                      while(i < token.argc){
                          printf("%s", token.argv[i]) ;
                          printf(" ");
                          i +=1 ;
                      }
                      printf("&\n");
                  }
                
                break;
            }
            
       case BUILTIN_QUIT  ://quit has to not terminate bg job
          printf("Kishor QUIT THIS");
          raise(SIGKILL);
          break; /* optional */

       case BUILTIN_FG :
            ;
            int intpd = pidparsebgfg(token); //parse pid from cmdline
            sigemptyset(&mask);
            sigfillset(&mask);
            
            sigprocmask(SIG_BLOCK, &mask, NULL);
            struct job_t *cur_job = getjobpid(job_list, intpd);
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            
            if(cur_job == NULL){
                printf("Supplied pid/jid not found \n");
                return;
            }  
            //Send SIGCONT to stopped pid
            kill(-intpd, SIGCONT); 
            //Change state to FG
            cur_job->state = FG;
            //Have to wait for the pid to finish and delete it from job list
            waitpid(intpd, &status, WUNTRACED);
            
            sigfillset(&mask);
            sigprocmask(SIG_BLOCK, &mask, NULL);
            deletejob(job_list, intpd);
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            
            break; 

       case BUILTIN_BG :
            ;
            sigemptyset(&mask);
            
            //get the pid to run in BG
            int intpid = pidparsebgfg(token);
            sigfillset(&mask);
            sigprocmask(SIG_BLOCK, &mask, NULL);
            struct job_t* curr_job = getjobpid(job_list, intpid);
           
            if(curr_job == NULL){
                printf("Supplied pid/jid not found \n");
                return;
            }
            
            //Send SIGCONT to stopped pid
            kill(-intpid, SIGCONT); 
            //Change child state in job list to BG 
            curr_job->state = BG;
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            
            printf("[%d] (%d) %s \n", curr_job->jid, curr_job->pid, curr_job->cmdline);
            //Asynchronous so job deleted in SIGCHLD handler when it finishes running
            break; /* optional */

       case BUILTIN_JOBS :
          //printf("Listing now\n");
          sigemptyset(&mask);
            
          sigfillset(&mask);
          sigprocmask(SIG_BLOCK, &mask, NULL);
          listjobs(job_list, STDOUT_FILENO);
          sigprocmask(SIG_UNBLOCK, &mask, NULL);
            
          break; 
    }
    
    //My code goes here.
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * <What does sigchld_handler do?>
 */

int pidparsebgfg(struct cmdline_tokens token){

            sigset_t mask;
            sigemptyset(&mask);
            
            char *pidorjid = token.argv[1];
            char * charjid;
            char * charpid;
            int intpid;
            
            if(pidorjid[0] == 37){//JID
                charjid = &pidorjid[1];
                int intjid = atoi(charjid);
                sigfillset(&mask);
                sigprocmask(SIG_BLOCK, &mask, NULL);
                struct job_t* job= getjobjid(job_list, intjid); 
                sigprocmask(SIG_UNBLOCK, &mask, NULL);
                
                if(job == NULL){
                    return -1;
                }
                intpid = job->pid;
            }
            else{//PID
                charpid = &pidorjid[0];
                intpid = atoi(charpid);
            }
            return intpid;
}

void sigusr1handler(int sig){
    return;
}

void sigchld_handler(int sig) //gets called if child is stopped or terminated
{   
        sigset_t mask;
        sigemptyset(&mask);
    
        sigfillset(&mask);
        sigprocmask(SIG_BLOCK, &mask, NULL);
    //mask 
    int status;
    int pid = waitpid(-1, &status, WNOHANG);
    if(pid != -1 && pid != 0){ //-1 if child is reaped in parent, 0 if child was stopped
        if(WIFSIGNALED(status) != 0){//sigchld called because child died of a signal
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(job_list,pid), pid, WTERMSIG(status));
            
        }
        struct job_t *job = getjobpid(job_list, pid);
          if(job != NULL){
              deletejob(job_list, pid);    
          }
    }
    
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    fgjobrunning = false;
    raise(SIGUSR1);
    //unmask 
}

/* 
 * <What does sigint_handler do?>
 */
void sigint_handler(int sig) 
{    
        sigset_t mask;//mask set
        sigemptyset(&mask);
        sigfillset(&mask);
    
        sigprocmask(SIG_BLOCK, &mask, NULL);
    
    int childpid = fgpid(job_list);
    
    if(childpid != 0){
            //printf("About to kill the FG\n");
            kill(-childpid, SIGINT);
    }
     sigprocmask(SIG_UNBLOCK, &mask, NULL);
    return;
}

/*
 * <What does sigtstp_handler do?>
 */
void sigtstp_handler(int sig) 
{
        sigset_t mask;//mask set
        sigemptyset(&mask);
        sigfillset(&mask);
    
        sigprocmask(SIG_BLOCK, &mask, NULL);
    
    int childpid = fgpid(job_list);
    if (childpid != 0){
        kill(-childpid, SIGTSTP);
    }
    
    sigprocmask(SIG_UNBLOCK, &mask, NULL);//unmask blocked signals
    return;
}

void sigkill_handler(int sig)
{
    //printf("KIlled it ");
    return;
}

void sigterm_handler(int sig){
        sigset_t mask;//mask set
        sigemptyset(&mask);
        sigfillset(&mask);
    
        sigprocmask(SIG_BLOCK, &mask, NULL);
    int i;
    for(i=0; i <MAXJOBS; i++){
        if(job_list[i].pid !=0){
            int pid = job_list[i].pid;
            if(job_list[i].state == BG){
                kill(-pid, SIGTERM);
            }
        }
        else{
            sigprocmask(SIG_UNBLOCK, &mask, NULL);//unmask blocked signals
            return;
        }
    }
     sigprocmask(SIG_UNBLOCK, &mask, NULL);//unmask blocked signals
    return;
}
