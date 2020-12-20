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

#include "config.h"

struct servlist {
    char services[MAXSERVICES][256];
    int servc;
};

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
           "kanrisha restart service - restart service\n");
}

struct servlist get_running_servs() {
    struct servlist servslist;

    struct dirent* dent;
    DIR* srcdir = opendir("/etc/kanrisha.d/available/");
    if(srcdir == NULL) {
        fprintf(stderr, "error: something went really wrong\n");
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
            char* pidfname = malloc(sizeof(char) * (32 + strlen(dent->d_name)));

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
        fprintf(stderr, "error: something went really wrong\n");
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
            fprintf(stderr, "error: something went really wrong\n");
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

            if(S_ISDIR(st.st_mode)) printf("%s\n", dent->d_name);
        }
        closedir(srcdir);
    }
    return 0;
}

int showlog(char servname[]) {
    char* logfname = malloc(sizeof(char) * (32 + strlen(servname)));

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

    char* pidfname = malloc(sizeof(char) * (32 + strlen(servname)));
    char* logfname = malloc(sizeof(char) * (32 + strlen(servname)));

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

    free(pidfname);
    free(logfname);

    // printing the last lines of a file in C is a fucking nightmare when 
    // you care about compact, fast and beautiful code. we'll just execvp 
    // tail to deal with this absolutely hellish fuckery

    char* fullcmd[] = { "tail", "-n", STATUSLOGLEN, logfname, NULL };
    execvp(fullcmd[0], fullcmd);

    perror("execvp");

    return 126;
}

int enable_serv(char servname[]) {
    char* dirname = malloc(sizeof(char) * (32 + strlen(servname)));

    strcpy(dirname, "/etc/kanrisha.d/available/");
    strcat(dirname, servname);
    strcat(dirname, "/");

    if(access(dirname, F_OK) != -1) {
        char* targetdirname = malloc(sizeof(char) * (28 + strlen(servname)));
        strcpy(targetdirname, "/etc/kanrisha.d/enabled/");
        strcat(targetdirname, servname);
        if(symlink(dirname, targetdirname) != 0) {
            if (errno == EACCES) {
                fprintf(stderr, "error: missing permissions\n");
                return 1;
            } else if (errno == EEXIST) {
                fprintf(stderr, "error: %s is already enabled\n", servname);
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
    char* dirname = malloc(sizeof(char) * (28 + strlen(servname)));

    strcpy(dirname, "/etc/kanrisha.d/enabled/");
    strcat(dirname, servname);
    strcat(dirname, "/");

    if(access(dirname, F_OK) != -1) {
        if (unlink(dirname) != 0) {
            if (errno == EACCES || errno == EPERM) {
                fprintf(stderr, "error: missing permissions\n");
                return 1;
            }
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

    char* fname = malloc(sizeof(char) * (32 + strlen(servname)));
    char* pidfname = malloc(sizeof(char) * (32 + strlen(servname)));
    char* logfname = malloc(sizeof(char) * (32 + strlen(servname)));

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
                fprintf(stderr, "error: missing permissions\n");
                return 1;
            }
        } else {
            fprintf(stderr, "error: %s is already running\n", servname);
            return 1;
        }
    } else {
        fprintf(stderr, "error: %s doesn't exist\n", servname);
        return 1;
    }
    printf("service %s has been started\n", servname);

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
    printf("stopping service %s...\n", servname);

    char* fname = malloc(sizeof(char) * (32 + strlen(servname)));

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
                fprintf(stderr, "error: missing permissions\n");
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
            printf("service %s won't terminate, killing it\n", servname);
            if(kill((pid_t)pid, SIGKILL) != 0) {
                if (errno == EPERM) {
                    fprintf(stderr, "error: missing permissions\n");
                    return 1;
                }
            }
        }

        if (unlink(fname) != 0) {
            fprintf(stderr, "error: cannot delete pidfile. please remove it manually or problems will occur ( %s )\n", strerror(errno));
            return 1;
        }
    } else {
        fprintf(stderr, "error: %s isn't running\n", servname);
        return 1;
    }
    printf("service %s has been stopped\n", servname);

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
    char* fname = malloc(sizeof(char) * (32 + strlen(servname)));

    strcpy(fname, "/etc/kanrisha.d/available/");
    strcat(fname, servname);
    strcat(fname, "/pid");

    if(access(fname, F_OK|R_OK) != -1) {
        stop_serv(servname);
        start_serv(servname);
    } else {
        fprintf(stderr, "error: %s isn't running\n", servname);
        return 1;
    }
    printf("service %s has been restarted\n", servname);

    free(fname);

    return 0;
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
    } else {
        help();
    }
    return 0;
}
