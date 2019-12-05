/* --- secret UID --- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "command.h"
#include "executor.h"

#ifndef _WIN32_WINNT
#include <sys/wait.h>
#include <sysexits.h>
#include <err.h>

#else
#define fork() 0
#define wait(a) 0
#define WEXITSTATUS(a) 0
#define EX_OSERR -1
#define err(a, b) 0
#define pipe(a) 0
#endif

#define CMD_CD          "cd"
#define CMD_EXIT        "exit"
#define FILE_PERMISSION (0664)
#define FAILED_TO_EXEC  "Failed to execute %s\n"
#define AMBI_INPUT_RDR  "Ambiguous input redirect.\n"
#define AMBI_OUTPUT_RDR "Ambiguous output redirect.\n"

/* static void print_tree(struct tree *);todo delete */

static int execute_pipe(struct tree *t, const int *fdi, const int *fdo);

int execute(struct tree *t) {
    return execute_pipe(t, 0, 0);
}

static int execute_pipe(struct tree *t, const int *fdi, const int *fdo) {
#define first   (t->argv[0])
#define second  (t->argv[1])

    if (t->conjunction == AND) {
        if (!execute_pipe(t->left, fdi, fdo)) {
            return execute_pipe(t->right, fdi, fdo);
        }
        return EXIT_FAILURE;

    } else if (t->conjunction == PIPE) {
        pid_t pid;

        if ((pid = fork()) > 0) {               /* parent */
            int status;

            wait(&status);
            return WEXITSTATUS(status);
        } else if (pid == 0) {                  /* child */
            int   pfd[2];
            pid_t pid2;

            pipe(pfd);
            if ((pid2 = fork()) < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid2) {
                int status, result;

                close(pfd[1]);
                result = execute_pipe(t->right, pfd, fdo);
                close(pfd[0]);
                wait(&status);
                exit(WEXITSTATUS(status) && result);
                /* use && for both results */
            } else {
                int result;

                close(pfd[0]);
                result = execute_pipe(t->left, fdi, pfd + 1);
                close(pfd[1]);
                exit(result);
            }
        } else {
            perror("fork");
            return EXIT_FAILURE;
        }
    } else if (t->conjunction == SUBSHELL) {
        pid_t pid;

        if ((pid = fork()) > 0) {               /* parent */
            int status;

            wait(&status);
            return WEXITSTATUS(status);
        } else if (pid == 0) {                  /* child */
            exit(execute_pipe(t->left, fdi, fdo));
        } else {
            perror("fork");
            return EXIT_FAILURE;
        }
    } else if (t->conjunction == NONE) {
        if (strcmp(first, CMD_EXIT) == 0) {
            exit(EXIT_SUCCESS);

        } else if (strcmp(first, CMD_CD) == 0) {
            const char *dir = second ? second : getenv("HOME");

            if (chdir(dir) == -1) {
                perror(dir);
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;

        } else {
            pid_t pid;

            if ((pid = fork()) > 0) {           /* parent */
                int status;

                wait(&status);
                return WEXITSTATUS(status);

            } else if (pid == 0) {              /* child */
                int fd, ambi_rdr = 0;
                /* ambiguous redirect:
                 * 0: OK
                 * 1: input
                 * 2: output
                 * */

                if (t->input && fdi) {
                    ambi_rdr = 1;
                } else if (t->input) {
                    if ((fd = open(t->input, O_RDONLY)) < 0) {
                        err(EX_OSERR, "File opening (read) failed");
                    }
                    if (dup2(fd, STDIN_FILENO) < 0) {
                        err(EX_OSERR, "dup2 (read) failed");
                    }
                    close(fd);
                } else if (fdi) {
                    if (dup2(*fdi, STDIN_FILENO) < 0) {
                        err(EX_OSERR, "dup2 (read) failed");
                    }
                }
                if (t->output && fdo) {
                    ambi_rdr = 2;
                } else if (t->output) {
                    if ((fd = open(t->output,
                                   O_WRONLY | O_CREAT | O_TRUNC,
                                   FILE_PERMISSION)) < 0) {
                        err(EX_OSERR, "File opening (write) failed");
                    }
                    if (dup2(fd, STDOUT_FILENO) < 0) {
                        err(EX_OSERR, "dup2 (write) failed");
                    }
                    close(fd);
                } else if (fdo) {
                    if (dup2(*fdo, STDOUT_FILENO) < 0) {
                        err(EX_OSERR, "dup2 (write) failed");
                    }
                }

                if (ambi_rdr) {
                    printf(ambi_rdr == 2
                           ? AMBI_OUTPUT_RDR : AMBI_INPUT_RDR);
                    exit(EXIT_FAILURE);
                }

                execvp(first, t->argv);
                fprintf(stderr, FAILED_TO_EXEC, t->argv[0]);
                exit(EXIT_FAILURE);
            } else {
                perror("fork");
                return EXIT_FAILURE;
            }
        }
    }
#undef first
#undef second
    return EXIT_FAILURE;
}
/*

 clear
 make
 echo "\n\n\n"
(cd testing && ../d8sh < public00.in >! r0 && diff r0 public00.output)
(cd testing && ../d8sh < public01.in >! r1 && diff r1 public01.output)
(cd testing && ../d8sh < public02.in >! r2 && diff r2 public02.output)
(cd testing && ../d8sh < public03.in >! r3 && diff r3 public03.output)
(cd testing && ../d8sh < public04.in >! r4 && diff r4 public04.output)
(cd testing && ../d8sh < public05.in >! r5 && diff r5 public05.output)
(cd testing && ../d8sh < public06.in >! r6 && diff r6 public06.output)
(cd testing && ../d8sh < public07.in >! r7 && diff r7 public07.output)
(cd testing && ../d8sh < public08.in >! r8 && diff r8 public08.output)
(cd testing && ../d8sh < public09.in >! r9 && diff r9 public09.output)
(cd testing && ../d8sh < public10.in >! r10 && diff r10 public10.output)
(cd testing && ../d8sh < public11.in >! r11 && diff r11 public11.output)
(cd testing && ../d8sh < public12.in >! r12 && diff r12 public12.output)
(cd testing && ../d8sh < public13.in >! r13 && diff r13 public13.output)


 todo delete

static void print_tree(struct tree *t) {
    if (t != NULL) {
        print_tree(t->left);

        if (t->conjunction == NONE) {
            printf("NONE: %s, ", t->argv[0]);
        } else {
            printf("%s, ", conj[t->conjunction]);
        }
        printf("IR: %s, ", t->input);
        printf("OR: %s\n", t->output);

        print_tree(t->right);
    }
}*/

