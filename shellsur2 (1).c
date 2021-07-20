#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#define STD_WORD_LEN 20

int errid = 0;
int bg = 0;// флаг встреченного апрперсанда
int bg_pos = 0;// позиция первого встреченного амперсанда
char * out = NULL;
int flag_out = 0;
int redir_pos = 0;
int flag_out2 = 0;
int flag_outL = 0;
int f1 = 0;
int f2 = 0;
int f3 = 0;
int pipes = 0;
int pipe_pos = 0;
char * getword();

char * getword(){
    bool quote = false;
    int len = STD_WORD_LEN, c;

    if ((c = getchar()) == '\n' || c == EOF) return NULL;
    else if (isspace(c)) return getword();
    else if (c == '&') {
        char * str = malloc(2 * sizeof(char));
        *str = c;
        *(str + 1) = 0;
        bg = 1;
        return str;
    }else if (c=='|'){
        pipes++;
        char * str = malloc(2 * sizeof(char));
        *str = '|';
        *(str + 1) = 0;
        return str;
    }else if (c == '>'){
        char * str = malloc(3 * sizeof(char));
        str[0] = c;
        str[1] = 0;
        flag_out++;
        char p = getchar();
        if (p == '>') {
            str[1] = p;
            str[2] = 0;
            flag_out2++;
        } else ungetc(p, stdin);
        return str;
    } else if (c == '<') {
        char * str = malloc(3 * sizeof(char));
        str[0] = c;
        str[1] = 0;
        flag_outL++;
        char p = getchar();
        if (p == '<') {
            str[1] = p;
            str[2] = 0;
            flag_outL++;
        } else ungetc(p, stdin);
        return str;
    } else ungetc(c, stdin);

    char * str = malloc(len * sizeof(char)), * pos = str;
    while ((c = getchar()) != EOF && (!isspace(c) || quote)) {
        if (pos - str + 1 == len) {
            int offset = pos - str;
            str = realloc(str, len += 1);
            if (!str) perror("WORD ENTER");
            pos = str + offset;
        }
        if (c == '\n') {   // если выполнено, то точно qoute == 1, иначе не войдем в цикл
            printf("ERROR: unpaired quotes\n");
            errid = 1;
            free(str);
            return NULL;
        }
        if (c == '&' && !quote) {
            ungetc(c, stdin);
            bg = 1;
            break;
        }
        if (( c == '|' || c == '<' || c == '>') && !quote){
            ungetc(c, stdin);
            break;
        }
        if (c == '\"') {
            quote = !quote;
        } else {
            *pos = c;
            pos++;
        }
    }
    *pos = 0;
    if (c == '\n') {
        ungetc(c, stdin);
    }
    return str;
}


void print_done_pids() {
    int status;
    int pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("\n");
        if (WIFEXITED(status)) {
            printf("Process %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else {
            printf("Process %d signaled %d\n", pid, WTERMSIG(status));
        }
    }
}


void redirectproc(){

  if (out) {
     int fd[2];
      if (f2) {
          fd[0] = open(out, O_CREAT | O_WRONLY | O_APPEND, 0666);
       if (fd[0] == -1) {
           perror("REDIRECT");
           exit(53);
       } else dup2(fd[0], 1);
     }

     else if (f3) {
         fd[0] = open(out, O_RDONLY , 0666);
      if (fd[0] == -1) {
          perror("REDIRECT");
          exit(53);
      } else dup2(fd[0], 0);
    }

      else if (f1) {
           fd[0]= open(out, O_TRUNC | O_CREAT | O_WRONLY , 0666);
        if (fd[0] == -1) {
            perror("REDIRECT");
            exit(53);
        } else dup2(fd[0], 1);
      }
      close(fd[0]);
   }
}

int error_bg(char ** m){
    if (bg) {
        bg = 0;
        if (m[bg_pos + 1] == NULL) {
            free(m[bg_pos]);
            m[bg_pos] = NULL;
            bg_pos = 0;
            return 1;// запуск в фоновом режиме, ошибки нет
        } else return 2; // ошибка
    }
    return 0;// не в фоновом режиме
}

char ** parse_query(){
    int len = 10;
    char ** query = malloc(len * sizeof(char *)), ** pos = query;
    char * str;

    while ((str = getword())) {
        if (pos - query + 1 == len) {
            int offset = pos - query;
            query = realloc(query, (len += 1) * sizeof(char *));
            if (!query) perror("COMMAND PARSING");
            pos = query + offset;
        }
        if (*str == '&' && bg && !bg_pos) bg_pos = pos - query;
        else if (strcmp(str, ">")==0) redir_pos = pos - query;
        else if (strcmp(str, "<")==0) redir_pos = pos - query;
        else if (strcmp(str, ">>")==0) redir_pos = pos - query;
        else if (strcmp(str, "|")==0) pipe_pos = pos - query;
        *pos = str;
        pos++;
    }
    *pos = NULL;
    return query;
}

void piper(char ** m){
    int fd[2], i = 0;
    pipe(fd);
    pid_t pid;
    switch (pid = fork()) {
        case 0: dup2(fd[1], 1);
            close(fd[1]);
            close(fd[0]);
            execvp(m[0], m);
            exit(53);
        case -1: perror("FORK");
            exit(1);
        default: close(fd[1]);
    }
    while  (m[i] != NULL){
      i++;
    }
    switch (pid = fork()) {
        case 0: dup2(fd[0], 0);
            close(fd[0]);
            close(fd[1]);
            if (f2) {
                fd[0] = open(out, O_CREAT | O_WRONLY | O_APPEND, 0666);
             if (fd[0] == -1) {
                 perror("REDIRECT");
                 exit(53);
             } else dup2(fd[0], 1);
           }

           else if (f3) {
               fd[0] = open(out, O_RDONLY , 0666);
            if (fd[0] == -1) {
                perror("REDIRECT");
                exit(53);
            } else dup2(fd[0], 0);
          }

            else if (f1) {
                 fd[0]= open(out, O_TRUNC | O_CREAT | O_WRONLY , 0666);
              if (fd[0] == -1) {
                  perror("REDIRECT");
                  exit(53);
              } else dup2(fd[0], 1);
            }

            close(fd[0]);
            execvp(m[i+1], m);
            exit(53);
       case -1: perror("FORK");
            exit(1);
       default: close(fd[0]);
            close(fd[1]);
            break;
        }
    close(fd[0]);
}

void analyse(char ** m){
    if (pipes){
        free(m[pipe_pos]);
        m[pipe_pos] = NULL;
    }
    if ((flag_out == 1)|| (flag_outL == 1)){
        free(m[redir_pos]);
        m[redir_pos] = NULL;
        if (flag_out2 == 1){
            free(m[redir_pos]);
            m[redir_pos] = NULL;
        }
        if (redir_pos == 0) errid = 3;
        else if (m[redir_pos + 1]){
              out = m[redir_pos + 1];
              if (m[redir_pos + 2] && bg_pos!=redir_pos + 2) errid = 3;
            } else errid = 3;
       } else if ((flag_out != 0)||(flag_outL != 0)) errid = 2;
    f1 = flag_out;
    f2 = flag_out2;
    f3 = flag_outL;
    flag_out = 0;
    flag_out2 = 0;
    flag_outL = 0;

}

int main(){
    char ** m;
    int status = 0;
    int fd,pid;


    while (!feof(stdin)) {
        printf(">");
        m = parse_query();
        analyse(m);
        if (errid) {
            switch (errid) {
                case 2: printf("Too many redirects\n");
                    break;
                case 3: printf("Error in redirect syntax\n");
                    break;
            }
            errid = 0;
          } else if (pipes){

                piper(m);
                int cond = error_bg(m);
                wait(NULL);
                wait(NULL);
                if (cond == 0){
                   int status;
                        wait(&status);
                        if (WIFEXITED(status)) {
                            if (WEXITSTATUS(status) == 53) {
                                printf("ERROR!!!\n");
                                wait(NULL);
                            }
                        }

               }else{
                    print_done_pids();
                }
                free(out);
                out = NULL;
                pipes = 0;
        } else {
            if (m && m[0] && strcmp(m[0], "cd") == 0) {
                error_bg(m);
                if (m[1]) chdir(m[1]);
                else chdir(getenv("HOME"));
            } else {
                int cond = error_bg(m);
                int fd1;
                switch (pid = fork()) {
                    case -1: perror("CAN'T CREATE NEW PROCESS"); exit(2);
                        break;
                    case 0:
                    redirectproc();
                    if (m) execvp(m[0], m);
                      exit(53);
                      default:
                        if (cond == 0) {
                            waitpid(pid, &status, 0);
                            if (WIFEXITED(status) && m[0]) {
                                if (WEXITSTATUS(status) == 53) {
                                    printf("Error: command not found\n");
                                }
                            }
                        } else if (cond == 2) {
                            printf("MISSPLACED AMPERSAND!\n");
                            wait(NULL);
                        }
                        free(out);
                        out = NULL;
                }
            }
        }
        print_done_pids();
        free(m);
    }
    printf("\n");
    return 0;
}
