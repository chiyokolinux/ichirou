/**
 * kanrisha - a simple service manager, created for
 * the ichirou init system. it is part of ichirou.
 * Copyright (c) 2020-2021 Emily <elishikawa@jagudev.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
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
#include <syslog.h>

void malloc_fail();
void sys_perror(char *description);
void sys_eprintf(char *format, char *servname);
void sys_wprintf(char *format, char *servname);
void sys_iprintf(char *format, char *servname);
void sys_cprintf(char *format, unsigned char command, int retval);
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
    pid_t procid; /* same as /etc/kanrisha.d/available/<service>/pid */
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

void sys_perror(char *description) {
    int error = errno;
#ifdef WRITE_TO_OUTPUT
    fprintf(stderr, "%s: %s\n", description, strerror(error));
#endif
#ifdef WRITE_TO_SYSLOG
    syslog(LOG_DAEMON | LOG_ERR, "%s: %s\n", description, strerror(error));
#endif
}

void sys_eprintf(char *format, char *servname) {
#ifdef WRITE_TO_OUTPUT
    fprintf(stderr, format, servname);
#endif
#ifdef WRITE_TO_SYSLOG
    syslog(LOG_DAEMON | LOG_ERR, format, servname);
#endif
}

void sys_wprintf(char *format, char *servname) {
#ifdef WRITE_TO_OUTPUT
    fprintf(stderr, format, servname);
#endif
#ifdef WRITE_TO_SYSLOG
    syslog(LOG_DAEMON | LOG_WARNING, format, servname);
#endif
}

void sys_iprintf(char *format, char *servname) {
#ifdef WRITE_TO_OUTPUT
    printf(format, servname);
#endif
#ifdef WRITE_TO_SYSLOG
    syslog(LOG_DAEMON | LOG_NOTICE, format, servname);
#endif
}

void sys_cprintf(char *format, unsigned char command, int retval) {
#ifdef WRITE_TO_OUTPUT
    printf(format, command, retval);
#endif
#ifdef WRITE_TO_SYSLOG
    syslog(LOG_DAEMON | LOG_INFO, format, command, retval);
#endif
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
    if (srcdir == NULL) {
        perror("get_running_servs(): opendir");
        exit(1);
    }

    while ((dent = readdir(srcdir)) != NULL) {
        struct stat st;

        if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
            continue;

        if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
            perror("get_running_servs(): fstatat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            char* pidfname;
            if (!(pidfname = malloc(sizeof(char) * (32 + strlen(dent->d_name))))) malloc_fail();

            strcpy(pidfname, "/etc/kanrisha.d/available/");
            strcat(pidfname, dent->d_name);
            strcat(pidfname, "/pid");

            if (!access(pidfname, F_OK)) {
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
    if (srcdir == NULL) {
        perror("get_enabled_servs(): opendir");
        exit(1);
    }

    while ((dent = readdir(srcdir)) != NULL) {
        struct stat st;

        if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
            continue;

        if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
            perror("get_enabled_servs(): fstatat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
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
        if (srcdir == NULL) {
            perror("list(): opendir");
            return 1;
        }

        while ((dent = readdir(srcdir)) != NULL) {
            struct stat st;

            if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
                continue;

            if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
                perror("list(): fstatat");
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
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
    if (access(pidfname, F_OK) != -1) {
        FILE *pidf;
        pidf = fopen(pidfname, "r");
        if (pidf == NULL) {
            fprintf(stderr, "error: %s", strerror(errno));
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

    printf("%s - %s\n"
           "main pid: %d\n\n",
           servname, status, pid);

    /* printing the last lines of a file in C is a fucking nightmare when
       you care about compact, fast and beautiful code. we'll just execvp
       tail to deal with this absolutely hellish fuckery */

    char* fullcmd[] = { "tail", "-n", STATUSLOGLEN, logfname, NULL };
    execvp(fullcmd[0], fullcmd);

    perror("execvp");

    return 0;
}

int enable_serv(char servname[]) {
    char* dirname;
    if (!(dirname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    /* TODO: this may or may not be correct.
       tested command: sudo ln -s /etc/kanrisha.d/available/destlink /etc/kanrisha.d/enabled/ */
    snprintf(dirname, 32 + strlen(servname), "/etc/kanrisha.d/available/%s", servname);

    if (access(dirname, F_OK) != -1) {
        char* targetdirname;
        if (!(targetdirname = malloc(sizeof(char) * (28 + strlen(servname))))) malloc_fail();

        snprintf(dirname, 28 + strlen(servname), "/etc/kanrisha.d/enabled/%s", servname);

        if (symlink(dirname, targetdirname) != 0) {
            if (errno == EEXIST) {
                sys_eprintf("error: %s is already enabled\n", servname);
                return 1;
            } else {
                sys_perror("enable_serv(): symlink");
                return 1;
            }
        }
        free(targetdirname);
    } else {
        sys_eprintf("error: %s doesn't exist\n", servname);
        return 1;
    }
    sys_iprintf("service %s has been enabled\n", servname);

    free(dirname);

    return 0;
}

int disable_serv(char servname[]) {
    char* dirname;
    if (!(dirname = malloc(sizeof(char) * (28 + strlen(servname))))) malloc_fail();

    snprintf(dirname, 28 + strlen(servname), "/etc/kanrisha.d/enabled/%s", servname);

    if (access(dirname, F_OK) != -1) {
        if (unlink(dirname) != 0) {
            sys_perror("disable_serv(): unlink");
            return 1;
        }
    } else {
        sys_eprintf("error: %s is already disabled\n", servname);
        return 1;
    }
    sys_iprintf("service %s has been disabled\n", servname);

    free(dirname);

    return 0;
}

int start_serv(char servname[]) {
    sys_iprintf("starting service %s...\n", servname);

    char* fname;
    char* pidfname;
    char* logfname;
    if (!(fname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();
    if (!(pidfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();
    if (!(logfname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();

    snprintf(fname, 32 + strlen(servname), "/etc/kanrisha.d/available/%s/run", servname);
    snprintf(pidfname, 32 + strlen(servname), "/etc/kanrisha.d/available/%s/pid", servname);
    snprintf(logfname, 32 + strlen(servname), "/etc/kanrisha.d/available/%s/run", servname);

    for (int i = 0; i < service_count; i++) {
        if (!strcmp(services[i]->name, servname)) {
            sys_eprintf("error: %s is already running\n", servname);
            free(fname);
            free(pidfname);
            free(logfname);
            return 1;
        }
    }

    if (access(fname, F_OK|X_OK) != -1) {
        if (access(fname, F_OK|W_OK) != -1) {
            pid_t child_pid = fork();
            if (child_pid == 0) {
                char *const args[] = { "--run-by-kanrisha", "true", NULL };

                int fd;
                if ((fd = open(logfname, O_CREAT | O_WRONLY | O_TRUNC, LOGFILEPERMS)) < 0){
                    sys_perror("start_serv(): open");
                    free(fname);
                    free(pidfname);
                    free(logfname);
                    return -1;
                }

                dup2(fd, 1);
                close(fd);

                execvp(fname, args);
                sleep(3);
                stop_serv(servname);
                exit(-1);
            } else {
                /* save service data internally */
                struct service *started_serv = malloc(sizeof(struct service));
                started_serv->name = strdup(servname);
                started_serv->procid = child_pid;
                started_serv->restart_when_dead = 1;
                started_serv->restart_times = 0;
                started_serv->exited_normally = 0;

                services[service_count++] = started_serv;

                /* save pidfile on disk */
                FILE *fp;
                fp = fopen(pidfname, "w+");
                fprintf(fp, "%d\n", child_pid);
                fclose(fp);
            }
        } else {
            sys_eprintf("error: missing permissions\n", NULL);
            free(fname);
            free(pidfname);
            free(logfname);
            return 1;
        }
    } else {
        sys_eprintf("error: %s doesn't exist\n", servname);
        free(fname);
        free(pidfname);
        free(logfname);
        return 1;
    }
    sys_iprintf("service %s has been started\n", servname);

    free(fname);
    free(logfname);
    free(pidfname);

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
    sys_iprintf("stopping service %s...\n", servname);

    char* fname;
    if (!(fname = malloc(sizeof(char) * (32 + strlen(servname))))) malloc_fail();
    snprintf(fname, 32 + strlen(servname), "/etc/kanrisha.d/available/%s/pid", servname);

    int running = 0, i;
    for (i = 0; i < service_count; i++) {
        if (!strcmp(services[i]->name, servname)) {
            running = 1;
            break;
        }
    }

    if (running) {
        /* is this even running?? */
        int needtokillproc = 1;
        if (kill(services[i]->procid, SIGTERM) != 0) {
            if (errno == ESRCH) {
                needtokillproc = 0;
            } else {
                sys_perror("stop_serv(): kill");
                free(fname);
                return 1;
            }
        }

        /* kil, process */
        for (int i = 0; i < SIGKILLTIMEOUT; i++) {
            if (kill(services[i]->procid, SIGTERM) != 0) {
                if (errno == ESRCH) {
                    needtokillproc = 0;
                    break;
                } else {
                    sys_perror("stop_serv(): kill");
                    free(fname);
                    return 1;
                }
            }
        }
        if (needtokillproc) {
            sys_iprintf("service %s won't terminate, killing it\n", servname);
            if (kill(services[i]->procid, SIGKILL) != 0) {
                if (errno == EPERM) {
                    sys_perror("stop_serv(): kill");
                    free(fname);
                    return 1;
                }
            }
        }

        /* delete pidfile */
        if (unlink(fname) != 0) {
            sys_wprintf("warning: cannot delete pidfile. please remove it manually or problems will occur ( %s )\n", strerror(errno));
            free(fname);
            return 1;
        }
    } else {
        sys_eprintf("error: %s isn't running\n", servname);
        free(fname);
        return 1;
    }
    sys_iprintf("service %s has been stopped\n", servname);

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
        sys_eprintf("error: %s isn't running\n", servname);
        return 1;
    }
    sys_iprintf("service %s has been restarted\n", servname);

    return 0;
}

int rundaemon() {
    /* setup child watcher */
    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
        sys_perror("rundaemon(): signal");
    }

    /* init system logging stuff */
    openlog("kanrisha", LOG_PID, LOG_DAEMON);

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
            sys_perror("rundaemon(): open");
            return -1;
        }

        /* read command session */
        while (count != 0) {
            do {
                count = read(cmdfd, &buf, sizeof(unsigned char));
                if (*buf != '\0' && pos <= MAXSERVICES) {
                    command[pos++] = *buf;
                } else {
                    break;
                }
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
                    sys_eprintf("error: unrecognized command\n", NULL);
                    break;
            }
            sys_cprintf("command %#04x returned %d\n", command[0], retval);
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
                    sys_iprintf("service %s died, restarting\n", services[i]->name);
                    stop_serv(services[i]->name);
                    start_serv(services[i]->name);
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
