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
#include <sys/wait.h>
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
void sigchld_handler(int signo);
int daemon_send(unsigned char command, char servname[]);

#include "config.h"

struct servlist {
    char services[MAXSERVICES][256];
    int servc;
};

struct service {
    char *name; /* name of service, for restarting */
    pid_t procid; /* used to be /etc/kanrisha.d/available/<service>/pid */
    int restart_when_dead; /* should we (still) restart? */
    int restart_times; /* how many times was the service restarted? used to prevent 100% cpu from instantly dying services */
    int exited_normally; /* set if retval = 0 || stopped by kanrisha stop */
};

struct service **services;
int service_count = 0;

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
        perror("get_running_servs(): opendir");
        exit(1);
    }

    while((dent = readdir(srcdir)) != NULL) {
        for (int i = 0; i < service_count; i++) {
            strcpy(servslist.services[servslist.servc++], services[i]->name);
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
        perror("get_enabled_servs(): opendir");
        exit(1);
    }

    while((dent = readdir(srcdir)) != NULL) {
        struct stat st;

        if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;

        if(fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
            perror("get_enabled_servs(): fstatat");
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
            printf("%s\n", enabledservs.services[servid]);
        }
    } else if (only_running) {
        struct servlist runningservs = get_running_servs();
        for (int servid = 0; servid < runningservs.servc; servid++) {
            printf("%s\n", runningservs.services[servid]);
        }
    } else {
        struct dirent* dent;
        DIR* srcdir = opendir("/etc/kanrisha.d/available/");
        if(srcdir == NULL) {
            perror("list(): opendir");
            return 1;
        }

        while((dent = readdir(srcdir)) != NULL) {
            struct stat st;

            if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
                continue;

            if(fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
                perror("list(): fstatat");
                continue;
            }

            if(S_ISDIR(st.st_mode)) {
                printf("%s\n", dent->d_name);
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

    char* logfname;
    if (!(logfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(logfname, "/etc/kanrisha.d/available/");
    strcat(logfname, servname);
    strcat(logfname, "/log");

    int running = 0, i;
    for (i = 0; i < service_count; i++) {
        if (!strcmp(services[i]->name, servname)) {
            running = 1;
            break;
        }
    }

    char status[12] = "not running";
    pid_t pid = running ? services[i]->procid : -1;
    if(running) {
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
        char* targetdirname;
        if (!(targetdirname = malloc(sizeof(char) * (28 + strlen(servname))))) malloc_fail();

        strcpy(targetdirname, "/etc/kanrisha.d/enabled/");
        strcat(targetdirname, servname);

        if(symlink(dirname, targetdirname) != 0) {
            if (errno == EEXIST) {
                fprintf(stderr, "error: %s is already enabled\n", servname);
                return 1;
            } else {
                perror("enable_serv(): symlink");
                return 1;
            }
        }
        free(targetdirname);
    } else {
        fprintf(stderr, "error: %s doesn't exist\n", servname);
        return 1;
    }
    printf("service %s has been enabled\n", servname);

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
            perror("disable_serv(): unlink");
            return 1;
        }
    } else {
        fprintf(stderr, "error: %s is already disabled\n", servname);
        return 1;
    }
    printf("service %s has been disabled\n", servname);

    free(dirname);

    return 0;
}

int start_serv(char servname[]) {
    printf("starting service %s...\n", servname);

    char* fname;
    char* logfname;
    if (!(fname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    strcpy(fname, "/etc/kanrisha.d/available/");
    strcat(fname, servname);
    strcpy(logfname, fname);
    strcat(fname, "/run");
    strcat(logfname, "/log");

    for (int i = 0; i < service_count; i++) {
        if (!strcmp(services[i]->name, servname)) {
            fprintf(stderr, "error: %s is already running\n", servname);
            return 1;
        }
    }

    if(access(fname, F_OK|X_OK) != -1) {
        if(access(fname, F_OK|W_OK) != -1) {
            pid_t child_pid = fork();
            if (child_pid == 0) {
                char *const args[] = { "--run-by-kanrisha", "true", NULL };

                int fd;
                if((fd = open(logfname, O_CREAT | O_WRONLY | O_TRUNC, LOGFILEPERMS)) < 0){
                    perror("start_serv(): open");
                    return -1;
                }

                dup2(fd, 1);
                close(fd);

                execvp(fname, args);
                sleep(3);
                stop_serv(servname);
                exit(-1);
            } else {
                struct service *started_serv = malloc(sizeof(struct service));
                started_serv->name = strdup(servname);
                started_serv->procid = child_pid;
                started_serv->restart_when_dead = 1;
                started_serv->restart_times = 0;
                started_serv->exited_normally = 0;

                services[service_count++] = started_serv;
            }
        } else {
            fprintf(stderr, "error: missing permissions\n");
            return 1;
        }
    } else {
        fprintf(stderr, "error: %s doesn't exist\n", servname);
        return 1;
    }
    printf("service %s has been started\n", servname);

    free(fname);
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
    printf("stopping service %s...\n", servname);

    int running = 0, i;
    for (i = 0; i < service_count; i++) {
        if (!strcmp(services[i]->name, servname)) {
            running = 1;
            break;
        }
    }

    if(running) {
        int needtokillproc = 1;
        if(kill(services[i]->procid, SIGTERM) != 0) {
            if (errno == ESRCH) {
                needtokillproc = 0;
            } else {
                perror("stop_serv(): kill");
                return 1;
            }
        }

        for (int i = 0; i < SIGKILLTIMEOUT; i++) {
            if(kill(services[i]->procid, SIGTERM) != 0) {
                if (errno == ESRCH) {
                    needtokillproc = 0;
                    break;
                } else {
                    perror("stop_serv(): kill");
                    return 1;
                }
            }
        }
        if (needtokillproc) {
            printf("service %s won't terminate, killing it\n", servname);
            if(kill(services[i]->procid, SIGKILL) != 0) {
                if (errno == EPERM) {
                    perror("stop_serv(): kill");
                    return 1;
                }
            }
        }
    } else {
        fprintf(stderr, "error: %s isn't running\n", servname);
        return 1;
    }
    printf("service %s has been stopped\n", servname);

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
    int running = 0, i;
    for (i = 0; i < service_count; i++) {
        if (!strcmp(services[i]->name, servname)) {
            running = 1;
            break;
        }
    }

    if (running) {
        stop_serv(servname);
        start_serv(servname);
    } else {
        fprintf(stderr, "error: %s isn't running\n", servname);
        return 1;
    }
    printf("service %s has been restarted\n", servname);

    return 0;
}

int rundaemon() {
    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
        perror("rundaemon(): signal");

    /* init fifo */
    mkfifo(CMDFIFOPATH, 0620);

    /* init variables */
    unsigned char *buf = NULL;
    int pos = 0;
    ssize_t count = 0;
    unsigned char *command = malloc(sizeof(char) * (MAXSERVICES + 18));

    /* start all enabled, since `kanrisha daemon` will probably only be run on boot. */
    start_all();

    /* main command loop */
    while (1) {
        /* open cmd fifo as read-only */
        int cmdfd = open(CMDFIFOPATH, O_RDONLY);
        if (cmdfd < 0) {
            perror("rundaemon(): open");
            return -1;
        }

        /* read command session */
        while (count != 0) {
            do {
                count = read(cmdfd, &buf, sizeof(unsigned char));
                if (*buf != '\0' && pos <= MAXSERVICES)
                    command[pos++] = *buf;
                else
                    break;
            } while (count != 0);

            /* fix overflow */
            if (pos >= MAXSERVICES) {
                command[pos] = '\0';
            }

            int retval = 0;
            char *servname = (char *)command;
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
                    fprintf(stderr, "error: unrecognized command\n");
                    break;
            }
            printf("command %#04x returned %d", command[0], retval);
        }

        /* close fifos and init next session */
        close(cmdfd);
    }

    unlink(CMDFIFOPATH);
}

void sigchld_handler(int signo) {
    if (signo == SIGCHLD) {
        pid_t chpid;
        int status;
        while ((chpid = waitpid(-1, &status, WNOHANG)) > 0) {
            int i;
            for (i = 0; i < service_count; i++) {
                if (chpid == services[i]->procid)
                    break;
            }

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                int retval = WEXITSTATUS(status);
                if (retval == 0 && !WIFSIGNALED(status)) {
                    services[i]->exited_normally = 1;
                } else {
                    services[i]->exited_normally = 0;
                }

                if (services[i]->restart_when_dead && services[i]->restart_times < MAXSVCRESTART) {
                    strcpy(output, "");
                    stop_serv(services[i]->name);
                    start_serv(services[i]->name);
                    strcpy(output, "");
                    services[i]->restart_times++;
                }
            }
        }
    }
}

int daemon_send(unsigned char command, char servname[]) {
    /* open cmd fifo as write-only */
    int cmdfd = open(CMDFIFOPATH, O_WRONLY);
    if (cmdfd < 0) {
        if (errno == ENOENT) {
            fprintf(stderr, "could not connect to kanrisha daemon, it is most likely not running.\n");
            return -3;
        }
        perror("open");
        return -1;
    }

    write(cmdfd, &command, sizeof(unsigned char));

    if (servname != NULL) {
        write(cmdfd, servname, sizeof(char) * (strlen(servname) + 1));
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2 && argc != 3) {
        help();
        return 1;
    }
    /* i know that this is terrible code. too bad! */
    if (!strcmp(argv[1], "start") && argc == 3) {
        return daemon_send(0x1A, argv[2]);
    } else if (!strcmp(argv[1], "start") && argc == 2) {
        return daemon_send(0x1B, NULL);
    } else if (!strcmp(argv[1], "stop") && argc == 3) {
        return daemon_send(0x1C, argv[2]);
    } else if (!strcmp(argv[1], "stop") && argc == 2) {
        return daemon_send(0x1D, NULL);
    } else if (!strcmp(argv[1], "restart") && argc == 3) {
        return daemon_send(0x1E, argv[2]);
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
        return daemon_send(0x4A, argv[2]);
    } else if (!strcmp(argv[1], "disable") && argc == 3) {
        return daemon_send(0x4B, argv[2]);
    } else if (!strcmp(argv[1], "daemon") && argc == 2) {
        return rundaemon();
    } else {
        help();
    }
    return 0;
}
