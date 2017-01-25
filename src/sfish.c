#include "../include/sfish.h"

int prev_return_code;
int user_flag, machine_flag;
int user_bold, machine_bold;
int user_color, machine_color;
char *user_name;
char *cmd, *parse_cmd;
char hyphen[8];
char last_working_directory[256];
char machine_name[256];
char sfish_prompt[256];
sf_job* job_head = NULL;
int job_id = 1;

pid_t sfish_pgid;
struct termios sfish_tmodes;
int sfish_terminal;
int sfish_SPID = -1;

int main(int argc, char** argv){
    rl_bind_key(2, sfish_store_pid); /** ctrl + B */
    rl_bind_key(7, sfish_get_pid);  /** ctrl + G */
    rl_bind_key(8,  sfish_print_menu);  /** ctrl + H */
    rl_bind_key(16, sfish_print_info);  /** ctrl + P */
    prev_return_code = 0;
    init_sfish();
    //DO NOT MODIFY THIS. If you do you will get a ZERO.
    rl_catch_signals = 0;
    //This is disable readline's default signal handlers, since you are going
    //to install your own.
    user_flag = machine_flag = 1;
    user_bold = machine_bold = 0;
    user_color = machine_color = 7; /** 7 is white*/
    user_name = getenv("USER");
    gethostname(machine_name, sizeof(machine_name));
    

    prompt_cat();
    while((cmd = readline(sfish_prompt)) != NULL){
        if(!cmd){
            continue;
        }
        else{
            add_history(cmd);
        }
        char* sfish_cmd = strdup(cmd);
        strtok(sfish_cmd, " ");
        parse_cmd = parse_builtin(cmd);
        switch(hash(sfish_cmd)){
            case EXIT:
                free(sfish_cmd);
                return EXIT_SUCCESS;
            case QUIT:
                free(sfish_cmd);
                return EXIT_SUCCESS;
            case CD:
                sfish_cd(parse_cmd);
                SFISH_BREAK
            case HELP:
                sfish_print_menu(0,0);
                SFISH_BREAK
            case PWD:
                sfish_pwd(parse_cmd);
                SFISH_BREAK
            case CHPMT:
                sfish_chpmt(parse_cmd);
                SFISH_BREAK
            case CHCLR:
                sfish_chclr(parse_cmd);
                SFISH_BREAK
            case PRT:
                sfish_prt(parse_cmd);
                SFISH_BREAK
            case JOBS:
                sfish_jobs();
                SFISH_BREAK
            case FG:
                sfish_fg(parse_cmd);
                SFISH_BREAK
            case BG:
                sfish_bg(parse_cmd);
                SFISH_BREAK
            case KILL:
                sfish_kill(parse_cmd);
                SFISH_BREAK
            case DISOWN:
                sfish_disown(parse_cmd);
                SFISH_BREAK
            default:
                if(cmd == NULL || strcmp(cmd, "") == 0 || strcmp(cmd, " ") == 0){
                    SFISH_BREAK
                }
                sf_exe(cmd);
        }
        //All your debug print statments should be surrounded by this #ifdef
        //block. Use the debug target in the mainkefile to run with these enabled.
        #ifdef DEBUG
            fprintf(stderr, "Length of command entered: %ld\n", strlen(cmd));
        #endif
        //You WILL lose points if your shell prints out garbage values.
        //free(user_name);
    }
    //Don't forget to free allocated memory, and close file descriptors.f
    //WE WILL NOT CHECK VALGRIND!
    return EXIT_SUCCESS;
}



/**
* Hash table used to hash string into long int. It will be easier to use switch 
* compare to calling strcmp 20 times.
* 
* Param@ str - command that gets hash to long int
*/
unsigned long hash(char* str){
    unsigned long key = 4381;
    int c;
    while((c = *str++)){
        key = ((key << 5) + key) + c;
    }
    return key;
}

/**
* Parse builtin functions
* Para@ commd - entire command from readline
* Return rest of command after ' ' (space)
*/
char* parse_builtin(char* commd){
    char *token = strdup(commd);
    strtok(token, " ");
    if(strlen(commd) == strlen(token)){
        prev_return_code = 0;
        return NULL;
    }
    char *s = (char*)malloc(256);
    strncpy(s, commd+strlen(token)+1, (strlen(commd) - strlen(token)));
    free(token);
    return s;
}

/****Builtin functions ****/
/**
 * Prints the HELP menu.
 */
int sfish_print_menu(int jjjj, int k){
    int i;
    for(i = 0; i < 15; i++){
        fprintf(stdout, "%s", help_menu[i]);   
    }
    rl_on_new_line();
    prev_return_code = 0;
    return 0;
}
int sfish_print_info(int jjjj, int k){
    int i;
    sf_job *j;
    for(i = 0; i < 14; i++){
        fprintf(stdout, "%s", info_menu[i]);   
    }
    fprintf(stdout, "----Number of Commands Run----\n"); 
    fprintf(stdout, "%d\n", (job_id-1));
    fprintf(stdout, "----Process Table----\n");
    fprintf(stdout, "PGID\tPID\tTIME\tCMD\t\n");
    for(j = job_head; j; j=j->next){
        fprintf(stdout, "%d\t%d\t00:00\t%s\n", j->pgid, j->first_p->pid, j->command);
    }  
    rl_on_new_line();
    prev_return_code = 0;
    return 0;
}

int sfish_store_pid(int jj, int k){
    if(job_head==NULL){
        sfish_SPID = -1;
    }
    else{
        sfish_SPID = job_head->pgid;
    }
    fprintf(stdout,"sfish_SPID set as %d\n", sfish_SPID);
    rl_on_new_line();
     prev_return_code = 0;
    return 0;
}
int sfish_get_pid(int kk, int k){
    if(sfish_SPID == -1){
        #ifdef DEBUG
            fprintf(stderr, "\nSPID does not exist and has been set to -1\n");
        #endif
        rl_on_new_line();
        RETURN_NEG_ONE
    }
    sf_job *j = find_job(sfish_SPID);
    if(!j){
        #ifdef DEBUG
            fprintf(stderr, "\nNo such job\n");
        #endif
        rl_on_new_line();
        RETURN_NEG_ONE
    }
    if(!j->foreground){
        /** send SIGSTOP */
        if(kill(- j->pgid, SIGSTOP) < 0){
            #ifdef DEBUG
                fprintf(stderr, "\nkill(SIGTSTP) failed\n");
            #endif
            rl_on_new_line();
            RETURN_NEG_ONE
        }
        fprintf(stdout,"\n[%d] %d stopped by signal %d\n",j->id, j->pgid, SIGSTOP);
    }  
    else{
        fprintf(stdout,"\n");
    }
    rl_on_new_line();
    return 0;
}
/**
* Changes the current working directory of the shell by using the chdir() system call.
* Para@ dir - directory changing to
* Return 0 on successful directory change, -1 if unccessful
*/
int sfish_cd(char* dir){
    char current[256];
    if(dir == NULL){
        if(getcwd(last_working_directory, sizeof(last_working_directory)) == NULL){
            #ifdef DEBUG
                fprintf(stderr, "sfish: cd: Fail getting CWD\n");
            #endif
            RETURN_NEG_ONE
        }
        /** if the parameter if null, change dir to home */
        if((chdir(getenv("HOME"))) != 0){
            #ifdef DEBUG
                fprintf(stderr, "sfish: cd: Fail redirect to HOME\n");
            #endif
            RETURN_NEG_ONE
        }   
    }else if(strcmp(dir, "-") == 0){
        if(strcmp(last_working_directory, "") == 0){
            #ifdef DEBUG
                fprintf(stderr, "sfish: cd: OLDPWD not set\n");
            #endif
            RETURN_NEG_ONE
        }
        else if(getcwd(current, sizeof(current)) != NULL){
            if((chdir(last_working_directory)) != 0){
                #ifdef DEBUG
                    fprintf(stderr, "sifh: cd: File redirect to PWD\n");
                #endif
                RETURN_NEG_ONE
            }
            strcpy(last_working_directory, current);
        }
        else{
            #ifdef DEBUG
                fprintf(stderr, "sfish: cd: CWD not set\n");
            #endif
            RETURN_NEG_ONE
        }
    }else{
        if((getcwd(last_working_directory, sizeof(last_working_directory))) == NULL){
            #ifdef DEBUG
                fprintf(stderr, "sfish: PWD not set\n");
            #endif
            RETURN_NEG_ONE
        }
        /** else just change dir */
        if((chdir(dir)) !=0){
            #ifdef DEBUG
                fprintf(stderr, "sfish: cd: %s: No such directory\n", dir);
            #endif
            RETURN_NEG_ONE
        }
    }
    free(dir);
    prev_return_code = 0;
    return 0;
}

/**
* Print absolute path of the working directory obtained through the getcwd(3) function
* Return 0 on successful, -1 otherwise
*/
int sfish_pwd(char* commd){
        char cwd[512];
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            fprintf(stderr, "%s\n", cwd);
            prev_return_code = 0;
            return 0;
        }
        else{
            #ifdef DEBUG
                fprintf(stderr, "sfish: pwd: Fail getting CWD\n");
            #endif
            RETURN_NEG_ONE
        }
}

/**
* Print previous command return code
*/
void sfish_prt(char* commd){
    fprintf(stdout, "%d\n", prev_return_code);
    prev_return_code = 0;
}

/**
* Helper function to initilize prompt
*/
void prompt_cat(){
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    memset(sfish_prompt, '\0', sizeof(sfish_prompt));
    strcat(sfish_prompt, "sfish");
    memset(hyphen, '\0', sizeof(hyphen));
    if(machine_flag == 1 || user_flag == 1){
        strcat(hyphen, "-");
        strcat(sfish_prompt, hyphen);
    }
    if(user_flag == 1){   
        if(user_bold == 1){
            strcat(sfish_prompt, bold_clr_list[user_color]);
        }
        else{
            strcat(sfish_prompt, clr_list[user_color]);
        }
        strcat(sfish_prompt, user_name);
        strcat(sfish_prompt, clr_list[7]);
        strcat(sfish_prompt, "@");
    }
    if(machine_flag == 1){
        if(machine_bold == 1){
            strcat(sfish_prompt, bold_clr_list[machine_color]);
        }
        else{
            strcat(sfish_prompt, clr_list[machine_color]);
        }
        strcat(sfish_prompt, machine_name);
        strcat(sfish_prompt, clr_list[7]);
    }
    strcat(sfish_prompt,":[");
    if(strcmp(getenv("HOME"), cwd)==0){
        strcat(sfish_prompt, "~");
    }
    else if(strlen(cwd) > strlen(getenv("HOME"))){
        char *s = (char*)malloc(512);
        char *p = strdup(cwd);
        strncpy(s,p+(strlen(getenv("HOME"))),(strlen(cwd) - strlen(getenv("HOME"))));
        strcat(sfish_prompt, s);
        free(s);
        free(p);
    }
    else{
        strcat(sfish_prompt, cwd);
    }
    strcat(sfish_prompt, "]> ");
}

/**
*change prompt setting
*param@ parsed user input that breaks to two parts: setting and toggle
*setting - user field in the prompt or machine field in the prompt
*toggle - 0 for disable or 1 for enable
*/
int sfish_chpmt(char* user_input){
    char *setting = strdup(user_input);
    strtok(setting, " ");
    char *toggle = strtok(NULL, " ");
    if((strtok(NULL, " ")) != NULL){
        #ifdef DEBUG
            fprintf(stderr, "sfish: chpmt: too many arguments\n");
        #endif
        RETURN_NEG_ONE
    }
    if (setting == NULL || toggle == NULL){
        #ifdef DEBUG
            fprintf(stderr, "sfish: chpmt: Invaild argument\n");
        #endif
        RETURN_NEG_ONE
    }    
    int tog = 99;
    if(strcmp(toggle,"0") == 0)
        tog = 0;
    else if(strcmp(toggle, "1") == 0)
        tog = 1;
    if(tog != 0 && tog != 1){
        #ifdef DEBUG
            fprintf(stderr, "sfish: chpmt: Invaild argument\n");
        #endif
        RETURN_NEG_ONE
    }   
    if(strcmp(setting, "user") == 0){
        if(tog == 0)
            user_flag = 0;
        else
            user_flag = 1;
        prev_return_code = 0;
        return 0;
    }   
    else if(strcmp(setting, "machine") == 0){
        if(tog == 0)
            machine_flag = 0;
        else
            machine_flag = 1;
        prev_return_code = 0;
        return 0;
    }
    else{
        #ifdef DEBUG
            fprintf(stderr, "sfish: chpmt: Invaild argument\n");
        #endif
        RETURN_NEG_ONE
    }
}

/**
*change prompt color
*setting - user field in the prompt or machine field in the prompt
*color - color you want
*bold - 0 for disable or 1 for enable 
*/
int sfish_chclr(char* user_input){
    char *setting = strdup(user_input);
    strtok(setting, " ");
    char *clr = strtok(NULL, " ");
    char *bold = strtok(NULL, " ");
    if((strtok(NULL, " ")) != NULL){
        #ifdef DEBUG
            fprintf(stderr, "sfish: chclr: too many arguments\n");
        #endif
        RETURN_NEG_ONE
    }
    if (setting == NULL || clr == NULL || bold == NULL){
        #ifdef DEBUG
            fprintf(stderr, "sfish: chclr: Invaild argument\n");
        #endif
        RETURN_NEG_ONE
    }
    int bold_f = 99;
    if(strcmp(bold,"0") == 0)
        bold_f = 0;
    else if(strcmp(bold, "1") == 0)
        bold_f = 1;
    if(bold_f != 0 && bold_f != 1){
        #ifdef DEBUG
            fprintf(stderr, "sfish: chclr: Invaild argument\n");
        #endif
        RETURN_NEG_ONE
    }
    if(strcmp(setting, "user") == 0){
        if(bold_f == 0)
            user_bold = 0;
        else
            user_bold = 1;
        switch(hash(clr)){
            case RED:
                user_color = red;
                break;
            case BLUE:
                user_color = blue;
                break;
            case GREEN:
                user_color = green;
                break;
            case YELLOW:
                user_color = yellow;
                break;
            case CYAN:
                user_color = cyan;
                break;
            case MAGENTA:
                user_color = magenta;
                break;
            case BLACK:
                user_color = black;
                break;
            case WHITE:
                user_color = white;
                break;
            default:
                #ifdef DEBUG
                    fprintf(stderr, "sfish: chclr: Invaild argument\n");
                #endif
                RETURN_NEG_ONE
        }
    }   
    else if(strcmp(setting, "machine") == 0){
        if(bold_f == 0)
            machine_bold = 0;
        else
            machine_bold = 1;
        switch(hash(clr)){
            case RED:
                machine_color = red;
                break;
            case BLUE:
                machine_color = blue;
                break;
            case GREEN:
                machine_color = green;
                break;
            case YELLOW:
                machine_color = yellow;
                break;
            case CYAN:
                machine_color = cyan;
                break;
            case MAGENTA:
                machine_color = magenta;
                break;
            case BLACK:
                machine_color = black;
                break;
            case WHITE:
                machine_color = white;
                break;
            default:
                #ifdef DEBUG
                    fprintf(stderr, "sfish: chclr: Invaild argument\n");
                #endif
                RETURN_NEG_ONE
        }
    }
    else{
        #ifdef DEBUG
            fprintf(stderr, "sfish: chclr: Invaild argument\n");
        #endif
        RETURN_NEG_ONE
    }
    prev_return_code = 0;
    return 0;
}

int sf_exe(char* commd){
   /** check if contain slash */
    const char* slash = "/";
    if(strstr(commd, slash)){
        /**  conatin slash */
        struct stat buf;
        /** parse argument */
        char* cmmd = strdup(commd);
        char* check_file = strtok(cmmd, " ");
        int count = 0;
        /** check if file exist */
        if((stat(check_file, &buf)) == 0){
            // create job
            sf_process *p = calloc(1,sizeof(sf_process));
            sf_job *mj = calloc(1, sizeof(sf_job));
            mj->first_p = p;
            mj->stdin = 0;
            mj->stdout = 1;
            mj->stderr = 2;
            mj->command = commd;
            mj->id = job_id;
            mj->notified = 100;
            job_id++;
            char *a;
            cmmd = strdup(commd);
            if(strstr(cmmd, "&")){
                a = strtok(cmmd, "&");
                /** run in backg*/
                mj->foreground = 0;
            }
            else{
                a = commd;
                mj->foreground = 1;
            }
            for(a=strtok(a, " "); a!= NULL; a = strtok(NULL, " ")){
                    p->argv[count] = a;
                    count++;
            }
            add_job(mj);
            /** executaion */
            run_job(mj, mj->foreground);
            prev_return_code = 0;
            return 0;
        }
        else{
            /**not exits*/
            #ifdef DEBUG
                fprintf(stderr, "sfish: %s: No such file or directory\n", commd);
            #endif
            RETURN_NEG_ONE
        }
    }
    else{
        // find the execuatble in PATH environment
        char* sfish_in = NULL;
        char* sfish_out = NULL;
        char* sfish_errout = NULL;
        // create job
        char *s = strdup(commd);
        char *token;
        int processes_count =0;
        sf_process *phead = NULL;
        sf_job *mj = calloc(1, sizeof(sf_job));
        mj->stdin = 0;
        mj->stdout = 1;
        mj->stderr = 2;
        mj->command = commd;
        mj->id = job_id;
        mj->notified = 100;
        job_id++;
        if(strstr(commd, "&")){
            s = strtok(s, "&");
            mj->foreground = 0;
        }
        else{
            mj->foreground = 1;
        }
        for(token= strsep(&s, "|"); token!= NULL; token = strsep(&s, "|")){
            processes_count++;
            sf_process *p = calloc(1, sizeof(sf_process));
            int count = 0;
            if((!strstr(token,">")) && (!strstr(token, "<")) && (!strstr(token, "2>"))){
                for(token=strtok(token, " "); token!= NULL; token = strtok(NULL, " ")){
                    p->argv[count] = token;
                    count++;
                }
            }
            else if(strstr(token, "<") && processes_count == 1){
                if(!(strstr(token, ">"))){
                    //  in put redirect
                    char* a = strtok(token, "<");
                    char* b = strtok(NULL, "<");
                    for(a=strtok(a, " "); a!= NULL; a = strtok(NULL, " ")){
                        p->argv[count] = a;
                        count++;
                    }
                    for(b=strtok(b, " "); b!= NULL; b = strtok(NULL, " ")){
                        if((b=strtok(b, " "))){
                            sfish_in = b;
                        }
                        else{
                            sfish_in = strtok(NULL, " ");
                        }
                    }     
                }
                else{
                    char* a = strtok(token, "<");
                    char* b = strtok(NULL, ">");
                    char* c = strtok(NULL, ">");
                    for(a=strtok(a, " "); a!= NULL; a = strtok(NULL, " ")){
                        p->argv[count] = a;
                        count++;
                    }
                    for(b=strtok(b, " "); b!= NULL; b = strtok(NULL, " ")){
                        if((b=strtok(b, " "))){
                            sfish_in = b;
                        }
                        else{
                            sfish_in = strtok(NULL, " ");
                        }
                    }
                    for(c=strtok(c, " "); c!= NULL; c = strtok(NULL, " ")){
                        if((c=strtok(c, " "))){
                            sfish_out = c;
                        }
                        else{
                            sfish_out = strtok(NULL, " ");
                        }
                    }
                    int tt = open(sfish_out, O_RDWR|O_CREAT|O_TRUNC, 0644);
                    if(tt<0){
                        RETURN_NEG_ONE
                    }
                    mj->stdout = tt;
                }
                int t = open(sfish_in, O_RDWR, 0644);
                if(t<0){
                    RETURN_NEG_ONE
                }
                mj->stdin = t;
            }
            else if(strstr(token,"2>")){
                char* a = strtok(token, "2>");
                char* b = strtok(NULL, "2>");
                //  error out put redirect
                for(a=strtok(a, " "); a!= NULL; a = strtok(NULL, " ")){
                    p->argv[count] = a;
                    count++;
                }
                for(b=strtok(b, " "); b!= NULL; b = strtok(NULL, " ")){
                    if((b=strtok(b, " "))){
                        sfish_errout = b;
                    }
                    else{
                        sfish_errout = strtok(NULL, " ");
                    }
                }
                int t = open(sfish_errout, O_RDWR|O_CREAT, 0644);
                if(t<0){
                    RETURN_NEG_ONE
                }
                mj->stderr = t;
            }
            else if(strstr(token, ">")){
                char* a = strtok(token, ">");
                char* b = strtok(NULL, ">");
                //  out put redirect
                for(a=strtok(a, " "); a!= NULL; a = strtok(NULL, " ")){
                    p->argv[count] = a;
                    count++;
                }
                for(b=strtok(b, " "); b!= NULL; b = strtok(NULL, " ")){
                    if((b=strtok(b, " "))){
                        sfish_out = b;
                    }
                    else{
                        sfish_out = strtok(NULL, " ");
                    }
                }
                int t = open(sfish_out, O_RDWR|O_CREAT|O_TRUNC, 0644);
                if(t<0){
                    RETURN_NEG_ONE
                }
                mj->stdout = t;
            }   
            else{
                return -1;
            }
            sf_process *cur = phead;
            if(!phead){
                phead = p;
            }
            else{
                while(cur->next){
                    cur = cur->next;
                }
                cur->next = p;
            }
        }
        mj->first_p = phead;
        add_job(mj);
        run_job(mj, mj->foreground);
        return 0;
    }
}

/**
* list all jobs in the background
*/
void sfish_jobs(){
    sf_job *j;
    for (j = job_head; j; j=j->next){
        if(j->foreground){
            if(j->next){
                j = j->next;
            }
        }
        else{
            fprintf(stdout, "[%d]\t", j->id);
            if(j->notified == 0){
                fprintf(stdout, "Done\t\t%lu\t%s\n", (long)j->pgid, j->command);
            }
            else if(j->notified == 1) {
                fprintf(stdout, "Stopped\t\t%lu\t%s\n", (long)j->pgid, j->command);
            }
            else{
                fprintf(stdout, "Running\t\t%lu\t%s\n", (long)j->pgid, j->command);
            }
        }
    }
}

/**
*This builtin command should make the specified job number in your list go to the foreground
*(and send a signal to resume its execution if it is not currently running/stopped)
*/
void sfish_fg(char* commd){
    int jid_flag = 1;
    int jid = -1;
    int pid = -1;
    sf_job *j;
    if(!commd){
        prev_return_code = -1;
        return;
    }
    if(strstr(commd, "%")){
        char* token;
        token = strtok(commd, "%");
        jid = atoi(token);
    }
    else{
        jid_flag = 0;
        pid = atoi(commd);
    }
    /** if user entered JID */
    if(jid_flag){
        j = find_job_by_JID(jid);
    }
    else{ /** if user entered PID */
        j = find_job(pid);
    }
    if(!j){
        #ifdef DEBUG
            fprintf(stderr, "bg: No such job\n");
        #endif
        prev_return_code = -1;
        return;
    }
    continue_job(j, 1);
}

/**
*This builtin command should send a signal to the specified job in your job list to resume its
*execution in the background
*/
void sfish_bg(char* commd){
    int jid_flag = 1;
    int jid = -1;
    int pid = -1;
    sf_job *j;
    if(!commd){
        prev_return_code = -1;
        return;
    }
    if(strstr(commd, "%")){
        char* token;
        token = strtok(commd, "%"); 
        jid = atoi(token);
    }
    else{
        jid_flag = 0;
        pid = atoi(commd);
    }

    /** if user entered JID */
    if(jid_flag){
        j = find_job_by_JID(jid);
    }
    else{ /** if user entered PID */
        j = find_job(pid);
    }
    if(!j){
        #ifdef DEBUG
            fprintf(stderr, "bg: No such job\n");
        #endif
        prev_return_code = -1;
        return;
    }
    continue_job(j, 0);
}

/**
*This builtin command should send the specified signal to the process specified by PID. If no
*signal is specified you must send SIGTERM as default. The signal is denoted by its number
*ranging from 1 to 31 (inclusive)
*/
void sfish_kill(char* cmmd){
    char *s = strdup(cmmd);
    int count = 0;
    int pid = -1;
    int sig = -1;
    int jid_flag = 1;
    int jid = -1;
    sf_job *j;
    for(s = strtok(s," "); s; s=strtok(NULL, " ")){
        count++;
    }
    if(count == 1){ /** signal = default */
        if(strstr(cmmd, "%")){
            char* token = strdup(cmmd);
            token = strtok(token, "%"); 
            jid = atoi(token); // by jid
        }
        else{
            jid_flag = 0;
            pid = atoi(cmmd); // else by pid
        }
        /** if user entered JID */
        if(jid_flag){
            j = find_job_by_JID(jid);
        }
        else{ /** if user entered PID */
            j = find_job(pid);
        }
        if(!j){
            #ifdef DEBUG
                fprintf(stderr, "bg: No such job\n");
            #endif
            prev_return_code = -1;
            return;
        }
        if((kill(-j->pgid, SIGTERM))<0){
            #ifdef DEBUG
                fprintf(stderr, "sfish: kill: SIGTERM failed\n");
            #endif
        }
    }
    else if(count == 2){    /** user input = signal */
        s = strtok(cmmd, " ");
        sig = atoi(s);
        s = strtok(NULL," ");
        if(strstr(s, "%")){
            char* token = strdup(s);
            token = strtok(token, "%"); 
            jid = atoi(token); // by jid
        }
        else{
            jid_flag = 0;
            pid = atoi(cmmd); // else by pid
        }
        /** if user entered JID */
        if(jid_flag){
            j = find_job_by_JID(jid);
        }
        else{ /** if user entered PID */
            j = find_job(pid);
        }
        if(!j){
            #ifdef DEBUG
                fprintf(stderr, "bg: No such job\n");
            #endif
            prev_return_code = -1;
            return;
        }
        pid = atoi(s);
        if((kill(-j->pgid, sig))<0){
            #ifdef DEBUG
                fprintf(stderr, "sfish: kill: %d failed\n", sig);
            #endif
        }
    }
    else{
        #ifdef DEBUG
            fprintf(stderr, "sfish: kill: too many arguments\n");
        #endif
        prev_return_code = -1;
        return;
    }
    
}

void sfish_disown(char* cmmd){
    int jid_flag = 1;
    int jid = -1;
    int pid = -1;
    sf_job *j;
    if(!cmmd){ /** delete all job from background */
        for(j = job_head; j; j = j->next){
            if(j->foreground == 0){
                delete_job(j);
            }
        }
    }
    else{
        if(strstr(cmmd, "%")){
            char* token;
            token = strtok(cmmd, "%"); 
            jid = atoi(token);
        }
        else{
            jid_flag = 0;
            pid = atoi(cmmd);
        }

        /** if user entered JID */
        if(jid_flag){
            j = find_job_by_JID(jid);
        }
        else{ /** if user entered PID */
            j = find_job(pid);
        }
        if(!j){
            #ifdef DEBUG
                fprintf(stderr, "disown: No such job\n");
            #endif
            prev_return_code = -1;
            return;
        }
        if(j->foreground == 0){
            delete_job(j);
        }
        else{
            #ifdef DEBUG
                fprintf(stderr, "disown: Job is not running in background\n");
            #endif
        }
    }
}
/**
* Add job to end of job list
*/
void add_job(sf_job *j){
    if(!job_head){
        job_head = j;
    }
    else{
        sf_job *current = job_head;
        while(current->next){
            current = current->next;
        }
        current->next = j;
    }
}

void clear_job_list(){
    if(job_head == NULL){
        return;
    }
    sf_job *cur;
    for(cur=job_head;cur;cur=cur->next){
        delete_job(cur);
        free(cur);
    }
    job_head = NULL;
}

sf_job* find_job(int pid){
    if(!job_head){
        return NULL;
    }
    sf_job *cur;
    for(cur=job_head;cur;cur=cur->next){
        if((int)cur->pgid == pid){
            return cur;
        }
    }
    return NULL;
}

sf_job* find_job_by_JID(int jid){
    if(!job_head){
        return NULL;
    }
    sf_job *cur;
    for(cur=job_head;cur;cur=cur->next){
        if(cur->id == jid){
            return cur;
        }
    }
    return NULL;
}

void delete_job(sf_job *j){
    if(!job_head){
        return;
    }
    if(job_head == j){
        job_head = NULL;
    }
    else{
        sf_job *current = job_head;
        while(current->next){
            if(current->next == j){
                current->next = current->next->next;
            }
            else{
                current = current->next;
            }
        }
    }
    free(j);
}

void init_sfish(){
    sfish_terminal = STDIN_FILENO;
    /* Ignore interactive and job-control signals.  */
    //signal(SIGINT, &SIGINT_handle);
    //signal(SIGTSTP, &SIGTSTP_handle);
    signal(SIGCHLD, &SIGCHILD_handle);
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    /* Put ourselves in our own process group.  */
    sfish_pgid = getpid();
    if(setpgid(sfish_pgid, sfish_pgid) < 0){
        #ifdef DEBUG
            fprintf(stderr, "Can't put sfish in its own process group\n");
        #endif
        exit(1);
    }
}

/** 
* Check for processes that have status information available, without blocking. 
*/
void SIGCHILD_handle(){
    pid_t pid;
    int status;
    pid = waitpid(WAIT_ANY, &status, WUNTRACED|WNOHANG);
    if(pid > 0){
        sf_job *j = find_job(pid);
        if(j == NULL){
            return;
        }
        if(WIFEXITED(status)){
            j->notified = 0;
            prev_return_code = 0;
            delete_job(j);
        }
        else if(WIFSTOPPED(status)){ /** suspended */
            j->notified = 1;
            prev_return_code = -1;
        }
        else if(WIFSIGNALED(status)){ /** killed by signal */
            #ifdef DEBUG
                fprintf(stdout, "\n[%d] %d stopped by signal %d.\n",j->id, (int)pid, WTERMSIG(status));
            #endif
            delete_job(j);
            return;
        }
        else{
            #ifdef DEBUG
                fprintf(stdout, "weird error");
            #endif
        }
    }
}
void SIGINT_handle(){
    pid_t pid;
    int status;
    pid = waitpid(WAIT_ANY, &status, WUNTRACED|WNOHANG);
    if(kill(- pid, SIGINT) < 0){
        #ifdef DEBUG
            fprintf(stderr, "kill(SIGCONT) failed\n");
        #endif
    }
}

void SIGTSTP_handle(){
    pid_t pid;
    int status;
    pid = waitpid(WAIT_ANY, &status, WUNTRACED|WNOHANG);
    if(kill(- pid, SIGTSTP) < 0){
        #ifdef DEBUG
            fprintf(stderr, "kill(SIGTSTP) failed\n");
        #endif
    }
}
void run_job(sf_job *j, int fg){
    sf_process *p;
    pid_t pid;
    int mypipe[2], sf_in, sf_out;
    sf_in = j->stdin;
    for(p = j->first_p; p; p = p->next){/* Set up pipes  */
        if(p->next){
            if (pipe(mypipe) < 0){
                #ifdef DEBUG
                    fprintf(stderr, "error pipeing\n");
                #endif
                prev_return_code = -1;
                return;
            }
            sf_out = mypipe[1];
        }
        else{
            sf_out = j->stdout;
        }
        pid = fork();
        if(pid == 0){/** child*/
            run_proc(p, j->pgid, sf_in, sf_out, j->stderr, fg);
        }
        else if(pid < 0){/** fork failed.  */
            prev_return_code = -1;
            return;
        }
        else{/* parent  */
            p->pid = pid;
            if(!j->pgid){
                j->pgid = pid;
            }
            setpgid(pid, j->pgid);
        }
        /* Clean up pipes.  */
        if(sf_in != j->stdin){
            close(sf_in);
        }
        if(sf_out != j->stdout){
            close(sf_out);
        }
        sf_in = mypipe[0];   
    }
    if(fg){
        put_job_fg(j,0);
    }
    else{
        put_job_bg(j,0);
    }
    
}
void run_proc(sf_process *p, pid_t pgid,int sf_in, int sf_out, int sf_err,int fg){
    pid_t pid;
    pid = getpid();
    if(!pgid){
        pgid = pid;
    }
    setpgid(pid, pgid);
    if(fg){
       tcsetpgrp(sfish_terminal, pgid);
    }
    /* Set the handling for job control signals back to the default.  */
    signal(SIGCHLD, &SIGCHILD_handle);
    signal(SIGINT, &SIGINT_handle); /** ctrl+c */
    signal(SIGTSTP, &SIGTSTP_handle);   /** ctrl+z */
    //signal(SIGQUIT, SIG_IGN);
    //signal(SIGTTIN, SIG_IGN);
    //signal(SIGTTOU, SIG_IGN);
    /* Set the standard input/output channels of the new process.  */
    if(sf_in != STDIN_FILENO){
        dup2(sf_in, STDIN_FILENO);
        close(sf_in);
    }
    if(sf_out != STDOUT_FILENO){
        dup2(sf_out, STDOUT_FILENO);
        close(sf_out);
    }
    if(sf_err != STDERR_FILENO){
        dup2(sf_err, STDERR_FILENO);
        close(sf_err);
    }
    if(execvp(p->argv[0], p->argv)){
        #ifdef DEBUG
            fprintf(stderr, "error execuating\n");
        #endif
        prev_return_code = -1;
        exit(1);
    }
}

/**
* Put job j in the foreground.  
* If continue_job is nonzero, restore the saved terminal modes and send the process group a
* SIGCONT signal to wake it up before we block.  
*/
void put_job_fg (sf_job *j, int continue_job){
    j->foreground = 1;
    sf_process *p;
    pid_t pid;
    tcsetpgrp(sfish_terminal, j->pgid);
    /* continue */
    if(continue_job){
        if(kill(- j->pgid, SIGCONT) < 0){
            #ifdef DEBUG
                fprintf(stderr, "kill(SIGCONT) failed\n");
            #endif
        }
    }
    for(p = j->first_p; p; p = p->next){
        pid = p->pid;
    }
    waitpid(pid, &(p->status), WUNTRACED);
     /* Put the shell back in the foreground.  */
    tcsetpgrp(sfish_terminal, sfish_pgid);
}
/**
* Put a job in the background.  
* If the continue_job argument is true, send
* the process group a SIGCONT signal to wake it up.  
*/
void put_job_bg(sf_job *j, int continue_job){
    j->foreground = 0;
    /* continue job  */
    if(continue_job){
        if(kill(-j->pgid, SIGCONT) < 0){
            #ifdef DEBUG
                fprintf(stderr, "kill (SIGCONT) failed\n");
            #endif
        }
    }
    printf("[%d]\t%d\t%s &\n", j->id,(int)j->pgid, j->command);
}

/* Continue a job  */
void continue_job(sf_job *j, int fg){
    sf_process *p;
    for(p = j->first_p; p; p = p->next){
        p->stopped = 0;
    }
    j->notified = 9;
    if(fg){
        j->foreground = 1;
        put_job_fg(j, 1);
    }
    else{
        j->foreground = 0;
        put_job_bg(j, 1);
    }
}
