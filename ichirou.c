/* See LICENSE file for copyright and license details. */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LEN(x)	(sizeof (x) / sizeof *(x))
#define TIMEO	30

static void sigpoweroff(void);
static void sigreap(void);
static void sigreboot(void);
static void spawn(char *const []);
static void spawnwait(char *const argv[]);

static struct {
    int signal;
    void (*handler)(void);
} signalmap[] = {
    { SIGUSR1, sigpoweroff },
    { SIGCHLD, sigreap     },
    { SIGALRM, sigreap     },
    { SIGINT,  sigreboot   },
};

#include "config.h"

static sigset_t set;

int main(void) {
    /* init signal handlers */
    int signal;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);

    /* iterator var */
    size_t i;

    /* if not pid 1, exit */
    if (getpid() != 1) {
        fprintf(stderr, "This program must be run with PID 1\n");
    	return 1;
    }

    /* start system initialization script */
    chdir("/");
    spawnwait(rcinitfile);
    spawnwait(servstartcmd);
    spawnwait(rcpostinitfile);

    /* handle signals */
    while (1) {
        alarm(TIMEO);
        sigwait(&set, &signal);
        for (i = 0; i < LEN(signalmap); i++) {
            if (signalmap[i].signal == signal) {
            	signalmap[i].handler();
            	break;
            }
        }
    }

    /* will never be reached */
    return 0;
}

static void sigpoweroff(void) {
    /* stop services */
    spawnwait(servstopcmd);
    sigreap();

    /* ask processes to terminate */
    kill(-1, SIGTERM);
    sigreap();

    /* after SIGKILLTIMEOUT seconds, kill all processes */
    sleep(SIGKILLTIMEOUT);
    kill(-1, SIGKILL);
    sigreap();

    /* sync filesystems */
    sync();

    /* run rc.shutdown */
    spawnwait(rcshutdownfile);
    sigreap();

    /* power off system */
    if (vfork() == 0) {
        reboot(RB_POWER_OFF);
        _exit(EXIT_SUCCESS);
    }
    while (1)
        sleep(1);
}

static void sigreap(void) {
    /* reap children */
    while (waitpid(-1, NULL, WNOHANG) > 0);
    alarm(TIMEO);
}

static void sigreboot(void) {
    /* stop services */
    spawnwait(servstopcmd);
    sigreap();

    /* ask processes to terminate */
    kill(-1, SIGTERM);
    sigreap();

    /* after SIGKILLTIMEOUT seconds, kill all processes */
    sleep(SIGKILLTIMEOUT);
    kill(-1, SIGKILL);
    sigreap();

    /* sync filesystems */
    sync();

    /* run rc.shutdown */
    spawnwait(rcshutdownfile);
    sigreap();

    /* reboot system */
    if (vfork() == 0) {
        reboot(RB_AUTOBOOT);
        _exit(EXIT_SUCCESS);
    }
    while (1)
        sleep(1);
}

static void spawn(char *const argv[]) {
    switch (fork()) {
        case 0:
            sigprocmask(SIG_UNBLOCK, &set, NULL);
            setsid();
            execvp(argv[0], argv);
            perror("execvp");
            _exit(1);
        case -1:
            perror("fork");
    }
}

static void spawnwait(char *const argv[]) {
    pid_t chpid = fork();
    switch (chpid) {
        case 0:
            sigprocmask(SIG_UNBLOCK, &set, NULL);
            setsid();
            execvp(argv[0], argv);
            perror("execvp");
            _exit(1);
        case -1:
            perror("fork");
        default:
            waitpid(chpid, NULL, WUNTRACED);
    }
}
