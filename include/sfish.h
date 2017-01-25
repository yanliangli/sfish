#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>



/** macro for returing -1 with a error message */
#define RETURN_NEG_ONE prev_return_code = -1; return -1;
#define SFISH_BREAK	prompt_cat(); break;

/** Builtin functions hash table*/
#define HELP 5199371014
#define EXIT 5199283799
#define QUIT 5199711776
#define CD 4774276
#define PWD 157565992
#define PRT 157565843
#define CHPMT 171573426041	
#define CHCLR 171573411849
#define JOBS 5199453451
#define FG 4774378
#define BG 4774246
#define KILL 5199483177
#define DISOWN 5661963490865


/** color hash table */
#define RED 157567576
#define BLUE 5199163301
#define GREEN 171578516846
#define YELLOW 5662780335353
#define CYAN 5199212744
#define MAGENTA 186856090727194
#define BLACK 171572367194
#define WHITE 171597137054

/** Additional programs*/
typedef enum {red, blue, green, yellow, cyan, magenta, black, white} colorss;
/** The HELP menu. */
const char* help_menu[] = {
	"\nYan sfish, version 1.0.0-release (x86_64-pc-linux-gnu)\n",
	"These shell commands are defined internally.  Type 'help' to see this list.\n",
	"\nA star (*) next to a name means that the command is disabled.\n\n",
	" cd\t[dir]\n",
	" chclr\tsetting color bold\n",
	" chpmt\tsetting tpggle\n",
	" exit\t[n]\n",
	" help\t \n",
	" prt\t \n",
	" pwd\t \n",
	" jobs\t \n",
	" fg\tPID|JID\n",
	" bg\tPID|JID\n",
	" kill\t[signal] PID|JID\n",
	" disown\t[PID|JID]\n",
};
const char* info_menu[] = {
	"\n----Info----\n",
	"help\n",
	"prt\n",
	"----CTRL----\n",
	"cd\n",
	"chclr\n",
	"chpmt\n",
	"pwd\n",
	"exit\n",
	"----Info----\n",
	"bg\n",
	"fg\n",
	"disown\n",
	"jobs\n",
};
const char* clr_list[] = {"\x1b[31m", "\x1b[34m", "\x1b[32m", "\x1b[33m", "\x1b[36m", "\x1b[35m", "\x1b[30m", "\x1b[37m"};
const char* bold_clr_list[] = {"\033[1m\033[31m", "\033[1m\033[34m", "\033[1m\033[32m", "\033[1m\033[33m", "\033[1m\033[36m", "\033[1m\033[35m", "\033[1m\033[30m", "\033[1m\033[37m", };

/* A process is a single process.  */
typedef struct sf_process
{
	struct sf_process *next;	/* next process */
 	char *argv[128];			/* exec */
	pid_t pid;					/* process ID */
  	char completed;				/* true if process has completed */
  	char stopped;				/* true if process has stopped */
  	int status;					/* status value */
}sf_process;

/* A job is a pipeline of processes.  */
typedef struct sf_job
{
	struct sf_job *next;        /* next active job */
 	char *command;              /* command line, used for messages */
 	sf_process *first_p;     	/* list of processes in this job */
	pid_t pgid;                 /* process group ID */
 	int notified;               /* true if user told about stopped job */
 	struct termios tmodes;      /* saved terminal modes */
 	int stdin, stdout, stderr;  /* standard i/o channels */
 	int id;						/* job id*/
 	int foreground;				/* if job is in foreground */
} sf_job;

/**
* store the head of your jobs list in this variable.
*/
extern sf_job *job_head;
/**
* helper functions
*/
/**
* Hash table used to hash string into long int. It will be easier to use switch 
* compare to calling strcmp 20 times.
*/
unsigned long hash(char*);

/**
* initilize prompt
*/
void prompt_cat();
void add_job(sf_job*);
void add_bkg_job(sf_job*);
void delete_job(sf_job*);
void delete_bkg_job(sf_job*);
sf_job* find_job(int pid);
sf_job* find_job_by_JID(int jid);
void sfish_kill(char*);
void sfish_disown(char*);

/**
* Return rest of command after ' ' (space)
*/
char* parse_builtin(char*);

/**
*  builtin functions
*/

/**
* Changes the current working directory of the shell by using the chdir() system call.
* Return 0 on successful directory change, -1 otherwise
*/
int sfish_cd(char*);

/**
 * Prints the HELP menu.
 */
int sfish_print_menu(int, int);
/**
 * Prints the into .
 */
int sfish_print_info(int, int);
int sfish_store_pid(int, int);
int sfish_get_pid(int, int);

/**
* Print absolute path of the working directory obtained through the getcwd(3) function
* Return 0 on successful, -1 otherwise
*/
int sfish_pwd(char*);

/**
* Print the return code of the command that was last excuted
*/
void sfish_prt(char*);

/**
*change prompt setting
*param@ parsed user input that breaks to two parts: setting and toggle
*setting - user field in the prompt or machine field in the prompt
*toggle - 0 for disable or 1 for enable
*/
int sfish_chpmt(char*);

/**
*change prompt color
*setting - user field in the prompt or machine field in the prompt
*color - color you want
*bold - 0 for disable or 1 for enable 
*/
int sfish_chclr(char*);

/**
*Executes programs
*/
int sf_exe(char*);

/**
*  list all jobs in the background
*/
void sfish_jobs();
void sfish_fg(char*);
void sfish_bg(char*);


void init_sfish();
void run_job(sf_job*, int foreground);
void run_proc(sf_process*, pid_t pgid,int sf_in, int sf_out, int sf_err,int foreground);
void put_job_fg (sf_job*, int c);
void put_job_bg (sf_job*, int c);
void wait_for_job (sf_job*);
int mark_p_status (pid_t pid, int status);
void print_job_info (sf_job*, const char *status);
void mark_job_running (sf_job*);
void continue_job (sf_job*, int foreground);
int job_completed(sf_job*);
int job_stopped(sf_job*);
void clear_job_list();

/** signal handlers */
void SIGCHILD_handle();
void SIGINT_handle();
void SIGTSTP_handle();