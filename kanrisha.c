/* See LICENSE file for copyright and license details. */
/**
 *            kanrisha - a simple service manager
 *            created for the ichirou init system
 *
 * kanrisha list - list all available services
 * kanrisha list enabled - list enabled services
 * kanrisha list running - list running services
 * kanrisha log service - show latest log of service
 * kanrisha status service - show status of service
 * kanrisha enable service - enable service
 * kanrisha disable service - disable service
 * kanrisha start - start all enabled services
 * kanrisha start service - start service
 * kanrisha stop - stop all running services
 * kanrisha stop service - stop service
 * kanrisha restart service - restart service
**/

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

void malloc_fail();
void help();
struct servlist get_running_servs();
struct servlist get_enabled_servs();
int list(int only_enabled, int only_running);
int showlog(char servname[]);
int status(char servname[]);
int enable_serv(char servname[]);
int disable_serv(char servname[]);
int start_serv(char servname[]);
int start_all();
int stop_serv(char servname[]);
int stop_all();
int restart_serv(char servname[]);
int rundaemon();

#include "config.h"

struct servlist {
    char services[MAXSERVICES][256];
    int servc;
};

/* what should be sent through the socket? */
char *output;

void malloc_fail() {
    perror("malloc");
    exit(-1);
}

void help() {
    printf("           kanrisha - a simple service manager\n"
           "           created for the ichirou init system\n\n"
           "kanrisha list - list all available services\n"
           "kanrisha list enabled - list enabled services\n"
           "kanrisha list running - list running services\n"
           "kanrisha log service - show latest log of service\n"
           "kanrisha status service - show status of service\n"
           "kanrisha enable service - enable service\n"
           "kanrisha disable service - disable service\n"
           "kanrisha start - start all enabled services\n"
           "kanrisha start service - start service\n"
           "kanrisha stop - stop all running services\n"
           "kanrisha restart service - restart service\n"
           "kanrisha daemon - run main daemon in background\n");
}

struct servlist get_running_servs() {
    struct servlist servslist;

    struct dirent* dent;
    DIR* srcdir = opendir("/etc/kanrisha.d/available/");
    if(srcdir == NULL) {
        strcat(output, "error: something went really wrong\n");
        exit(1);
    }

    while((dent = readdir(srcdir)) != NULL) {
        struct stat st;

        if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;

        if(fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
            perror(dent->d_name);
            continue;
        }

        if(S_ISDIR(st.st_mode)) {
            char* pidfname;
            if (!(pidfname = malloc(sizeof(char) * (32 + strlen(dent->d_name))))) malloc_fail();

            strcpy(pidfname, "/etc/kanrisha.d/available/");
            strcat(pidfname, dent->d_name);
            strcat(pidfname, "/pid");

            if(access(pidfname, F_OK) == -1) {
                // servs[dir_count++] = dent->d_name;
                strcpy(servslist.services[servslist.servc++], dent->d_name);
            }
            free(pidfname);
        }
    }
    closedir(srcdir);
    return servslist;
}

struct servlist get_enabled_servs() {
    struct servlist servslist;

    struct dirent* dent;
    DIR* srcdir = opendir("/etc/kanrisha.d/enabled/");
    if(srcdir == NULL) {
        strcat(output, "error: something went really wrong\n");
        exit(1);
    }

    while((dent = readdir(srcdir)) != NULL) {
        struct stat st;

        if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;

        if(fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
            perror(dent->d_name);
            continue;
        }

        if(S_ISDIR(st.st_mode)) {
            // servs[dir_count++] = dent->d_name;
            strcpy(servslist.services[servslist.servc++], dent->d_name);
        }
    }
    closedir(srcdir);
    return servslist;
}

int list(int only_enabled, int only_running) {
    if (only_enabled) {
        struct servlist enabledservs = get_enabled_servs();
        for (int servid = 0; servid < enabledservs.servc; servid++) {
            strcat(output, enabledservs.services[servid]);
            strcat(output, "\n");
        }
    } else if (only_running) {
        struct servlist runningservs = get_running_servs();
        for (int servid = 0; servid < runningservs.servc; servid++) {
            strcat(output, runningservs.services[servid]);
            strcat(output, "\n");
        }
    } else {
        struct dirent* dent;
        DIR* srcdir = opendir("/etc/kanrisha.d/available/");
        if(srcdir == NULL) {
            strcat(output, "error: something went really wrong\n");
            return 1;
        }

        while((dent = readdir(srcdir)) != NULL) {
            struct stat st;

            if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
                continue;

            if(fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
                perror(dent->d_name);
                continue;
            }

            if(S_ISDIR(st.st_mode)) {
                strcat(output, dent->d_name);
                strcat(output, "\n");
            }
        }
        closedir(srcdir);
    }
    return 0;
}

int showlog(char servname[]) {
    char* logfname;
    if (!(logfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(logfname, "/etc/kanrisha.d/available/");
    strcat(logfname, servname);
    strcat(logfname, "/log");

    char* fullcmd[] = { "less", logfname, NULL };

    execvp(fullcmd[0], fullcmd);

    perror("execvp");
    return 1;
}

int status(char servname[]) {
    /**
     * destlink - running
     * main pid: 1337, uptime: 4h 32m 12s
     * cpu: 0.01%, mem: 12M
     * 
     * Linked target HOME (user/home) to /home/emily
     * Linked target ROOTHOME (system/roothome) to /root
     * Linked target MUSIC (user/music) to /run/media/emily/MusicHDD
     * Linked target VIDEOS (user/music) to /run/media/emily/MediaHDD/vids
     * Linked target PICTURES (user/music) to /run/media/emily/MediaHDD/pics
     * Moving queue
     * 
     * uptime, cpu and mem are difficult to calculate in C and I want to finish
     * the bases of chiyoko before dealing with these minor details.
    **/

    char* pidfname;
    char* logfname;
    if (!(pidfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();
    if (!(logfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(pidfname, "/etc/kanrisha.d/available/");
    strcat(pidfname, servname);
    strcpy(logfname, pidfname);
    strcat(pidfname, "/pid");
    strcat(logfname, "/log");

    char status[12] = "not running";
    pid_t pid = -1;
    if(access(pidfname, F_OK) != -1) {
        FILE *pidf;
        pidf = fopen(pidfname, "r");
        if (pidf == NULL) {
            strcat(output, "error: ");
            strcat(output, strerror(errno));
            strcat(output, "\n");
            return -1;
        }

        fscanf(pidf, "%d", &pid);
        fclose(pidf);

        kill(pid, 0);
        if (errno == ESRCH) {
            strcpy(status, "dead");
        } else {
            strcpy(status, "running");
        }
    }

    char tempout[1024];
    sprintf(tempout, "%s - %s\n"
           "main pid: %d\n\n",
           servname, status, pid);
    strcat(output, tempout);

    free(pidfname);
    free(logfname);

    // printing the last lines of a file in C is a fucking nightmare when 
    // you care about compact, fast and beautiful code. we'll just execvp 
    // tail to deal with this absolutely hellish fuckery

    // this needed to be removed during daemonization.

    // char* fullcmd[] = { "tail", "-n", STATUSLOGLEN, logfname, NULL };
    // execvp(fullcmd[0], fullcmd);

    // perror("execvp");

    return 0;
}

int enable_serv(char servname[]) {
    char* dirname;
    if (!(dirname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(dirname, "/etc/kanrisha.d/available/");
    strcat(dirname, servname);
    strcat(dirname, "/");

    if(access(dirname, F_OK) != -1) {
        char* targetdirname
        if (!(targetdirname = malloc(sizeof(char) * (28 + strlen(servname))))) malloc_fail();

        strcpy(targetdirname, "/etc/kanrisha.d/enabled/");
        strcat(targetdirname, servname);

        if(symlink(dirname, targetdirname) != 0) {
            if (errno == EACCES) {
                strcat(output, "error: missing permissions\n");
                return 1;
            } else if (errno == EEXIST) {
                strcat(output, "error: ");
                strcat(output, servname);
                strcat(output, " is already enabled\n");
                return 1;
            }
        }
        free(targetdirname);
    } else {
        strcat(output, "error: ");
        strcat(output, servname);
        strcat(output, " doesn't exist\n", servname);
        return 1;
    }
    strcat(output, "service ");
    strcat(output, servname);
    strcat(output, " has been enabled\n", servname);

    free(dirname);

    return 0;
}

int disable_serv(char servname[]) {
    char* dirname;
    if (!(dirname = malloc(sizeof(char) * (28 + strlen(servname))))) malloc_fail();

    strcpy(dirname, "/etc/kanrisha.d/enabled/");
    strcat(dirname, servname);
    strcat(dirname, "/");

    if(access(dirname, F_OK) != -1) {
        if (unlink(dirname) != 0) {
            if (errno == EACCES || errno == EPERM) {
                strcat(output, "error: missing permissions\n");
                return 1;
            }
        }
    } else {
        strcat(output, "error: ");
        strcat(output, servname);
        strcat(output, " is already disabled\n");
        return 1;
    }
    strcat(output, "service ");
    strcat(output, servname);
    strcat(output, " has been disabled\n", servname);

    free(dirname);

    return 0;
}

int start_serv(char servname[]) {
    strcat(output, "starting service ");
    strcat(output, servname);
    strcat(output, "...\n");

    char* fname;
    char* pidfname;
    char* logfname;
    if (!(fname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();
    if (!(pidfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();
    if (!(pidfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(fname, "/etc/kanrisha.d/available/");
    strcat(fname, servname);
    strcpy(pidfname, fname);
    strcpy(logfname, fname);
    strcat(fname, "/run");
    strcat(pidfname, "/pid");
    strcat(logfname, "/log");

    if(access(fname, F_OK|X_OK) != -1) {
        if(access(pidfname, F_OK) == -1) {
            if(access(fname, F_OK|W_OK) != -1) {
                pid_t child_pid = fork();
                if (child_pid == 0) {
                    char *const args[] = { "--run-by-kanrisha", "true", NULL };

                    int fd;
                    if((fd = open(logfname, O_CREAT | O_WRONLY | O_TRUNC, LOGFILEPERMS)) < 0){
                        perror("open");
                        return -1;
                    }

                    dup2(fd, 1);
                    close(fd);

                    execvp(fname, args);
                    sleep(3);
                    stop_serv(servname);
                    exit(-1);
                } else {
                    FILE *fp;
                    fp = fopen(pidfname, "w+");
                    fprintf(fp, "%d\n", child_pid);
                    fclose(fp);
                }
            } else {
                strcat(output, "error: missing permissions\n");
                return 1;
            }
        } else {
            strcat(output, "error: ");
            strcat(output, servname);
            strcat(output, " is already running\n");
            return 1;
        }
    } else {
        strcat(output, "error: ");
        strcat(output, servname);
        strcat(output, " doesn't exist\n", servname);
        return 1;
    }
    strcat(output, "service ");
    strcat(output, servname);
    strcat(output, " has been started\n", servname);

    free(fname);
    free(pidfname);
    free(logfname);

    return 0;
}

int start_all() {
    struct servlist servs = get_enabled_servs();
    int retval = 0;
    for (int i = 0; i < servs.servc; i++) {
        retval += start_serv(servs.services[i]);
    }
    return retval;
}

int stop_serv(char servname[]) {
    strcat(output, "stopping service ");
    strcat(output, servname);
    strcat(output, "...\n");

    char* fname;
    if (!(fname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(fname, "/etc/kanrisha.d/available/");
    strcat(fname, servname);
    strcat(fname, "/pid");

    if(access(fname, F_OK|R_OK) != -1) {
        FILE *fp;
        fp = fopen(fname, "r");

        int pid;
        fscanf(fp, "%d", &pid);

        fclose(fp);

        int needtokillproc = 1;
        if(kill((pid_t)pid, SIGTERM) != 0) {
            if (errno == EPERM) {
                strcat(output, "error: missing permissions\n");
                return 1;
            } else if (errno == ESRCH) {
                needtokillproc = 0;
            }
        }

        for (int i = 0; i < SIGKILLTIMEOUT; i++) {
            if(kill((pid_t)pid, SIGTERM) != 0) {
                if (errno == ESRCH) {
                    needtokillproc = 0;
                    break;
                }
            }
        }
        if (needtokillproc) {
            strcat(output, "service ");
            strcat(output, servname);
            strcat(output, " won't terminate, killing it\n");
            if(kill((pid_t)pid, SIGKILL) != 0) {
                if (errno == EPERM) {
                    strcat(output, "error: missing permissions\n");
                    return 1;
                }
            }
        }

        if (unlink(fname) != 0) {
            strcat(output, "error: cannot delete pidfile. please remove it manually or problems will occur ( ");
            strcat(output, strerror(errno));
            strcat(output, " )\n");
            return 1;
        }
    } else {
        strcat(output, "error: ");
        strcat(output, servname);
        strcat(output, " isn't running\n");
        return 1;
    }
    strcat(output, "service ");
    strcat(output, servname);
    strcat(output, " has been stopped\n", servname);

    free(fname);

    return 0;
}

int stop_all() {
    struct servlist servs = get_running_servs();
    int retval = 0;
    for (int i = 0; i < servs.servc; i++) {
        retval += stop_serv(servs.services[i]);
    }
    return retval;
}

int restart_serv(char servname[]) {
    char* fname;
    if (!(fname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(fname, "/etc/kanrisha.d/available/");
    strcat(fname, servname);
    strcat(fname, "/pid");

    if(access(fname, F_OK|R_OK) != -1) {
        stop_serv(servname);
        start_serv(servname);
    } else {
        strcat(output, "error: ");
        strcat(output, servname);
        strcat(output, " isn't running\n");
        return 1;
    }
    strcat(output, "service ");
    strcat(output, servname);
    strcat(output, " has been restarted\n", servname);

    free(fname);

    return 0;
}

int rundaemon() {
    /* init fifos */
    mkfifo(CMDFIFOPATH, 0620);
    mkfifo(OUTFIFOPATH, 0640);

    /* init variables */
    unsigned char *buf = NULL;
    int pos = 0;
    ssize_t count = 0;
    unsigned char *command = malloc(sizeof(char) * (MAXSERVICES + 18));
    output = malloc(sizeof(char) * (2048 + MAXSERVICES));
    strcpy(output, "");

    /* main command loop */
    while (1) {
        /* open cmd fifo as read-only */
        int cmdfd = open(CMDFIFOPATH, O_RDONLY);
        if (cmdfd < 0) {
            perror("open");
            return -1;
        }

        /* open output fifo as write-only */
        int outfd = open(OUTFIFOPATH, O_WRONLY);
        if (outfd < 0) {
            perror("open");
            return -1;
        }

        /* read command session */
        while (count != 0) {
            do {
                count = read(cmdfd, &buf, sizeof(char));
                if (buf != '\n')
                    command[pos++] = *buf;
                else
                    break;
            } while (count != 0);

            int retval = 0;
            char *servname = command;
            servname++;

            switch (command[0]) {
                case 0x1A:
                    retval = start_serv(servname);
                    break;
                case 0x1B:
                    retval = start_all();
                    break;
                case 0x1C:
                    retval = stop_serv(servname);
                    break;
                case 0x1D:
                    retval = stop_all();
                    break;
                case 0x1E:
                    retval = restart_serv(servname);
                    break;
                case 0x2A:
                    retval = status(servname);
                    break;
                case 0x2B:
                    retval = showlog(servname);
                    break;
                case 0x3A:
                    retval = list(0, 0);
                    break;
                case 0x3B:
                    retval = list(1, 0);
                    break;
                case 0x3C:
                    retval = list(0, 1);
                    break;
                case 0x4A:
                    retval = enable_serv(servname);
                    break;
                case 0x4B:
                    retval = disable_serv(servname);
                    break;
                default:
                    retval = 255;
                    break;
            }
            write(outfd, retval, sizeof(unsigned char));
            write(outfd, output, sizeof(char) * (strlen(output) + 1));
        }

        /* close fifos and init next session */
        close(cmdfd);
        close(outfd);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2 && argc != 3) {
        help();
        return 1;
    }
    /* i know that this is terrible code. too bad! */
    if (!strcmp(argv[1], "start") && argc == 3) {
        return start_serv(argv[2]);
    } else if (!strcmp(argv[1], "start") && argc == 2) {
        return start_all();
    } else if (!strcmp(argv[1], "stop") && argc == 3) {
        return stop_serv(argv[2]);
    } else if (!strcmp(argv[1], "stop") && argc == 2) {
        return stop_all();
    } else if (!strcmp(argv[1], "restart") && argc == 3) {
        return restart_serv(argv[2]);
    } else if (!strcmp(argv[1], "status") && argc == 3) {
        return status(argv[2]);
    } else if (!strcmp(argv[1], "log") && argc == 3) {
        return showlog(argv[2]);
    } else if (!strcmp(argv[1], "list") && argc == 2) {
        return list(0, 0);
    } else if (!strcmp(argv[1], "list") && !strcmp(argv[2], "enabled")) {
        return list(1, 0);
    } else if (!strcmp(argv[1], "list") && !strcmp(argv[2], "running")) {
        return list(0, 1);
    } else if (!strcmp(argv[1], "enable") && argc == 3) {
        return enable_serv(argv[2]);
    } else if (!strcmp(argv[1], "disable") && argc == 3) {
        return disable_serv(argv[2]);
    } else if (!strcmp(argv[1], "daemon") && argc == 2) {
        return rundaemon();
    } else {
        help();
    }
    return 0;
}
