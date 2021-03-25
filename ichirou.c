/**
 * ichirou - a basic init system for UNIX systems
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
**/

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LEN(x)  (sizeof (x) / sizeof *(x))
#define TIMEO   30

static void sigpoweroff(void);
static void sigfpoweroff(void);
static void sigffpoweroff(void);
static void sigreap(void);
static void sigreboot(void);
static void sigfreboot(void);
static void sigffreboot(void);
static void sighalt(void);
static void sigfhalt(void);
static void sighibernate(void);
static void spawn(char *const []);
static void spawnwait(char *const argv[]);

static struct {
    int signal;
    void (*handler)(void);
} signalmap[] = {
    { SIGUSR1, sigpoweroff   },
    { SIGUSR2, sigfpoweroff  },
    { SIGTTIN, sigffpoweroff },
    { SIGCHLD, sigreap       },
    { SIGALRM, sigreap       },
    { SIGINT,  sigreboot     },
    { SIGTERM, sigfreboot    },
    { SIGTTOU, sigffreboot   },
    { SIGQUIT, sighalt       },
    { SIGPWR,  sigfhalt      },
    { SIGHUP,  sighibernate  },
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

/**
 * this safely powers off the system
 * it stops services, asks processes to terminate,
 * kills non-terminating processes, syncs filesystems,
 * runs shutdown rcfile and then powers off the system.
**/
static void sigpoweroff(void) {
    /* stop services */
    spawnwait(servstopcmd);
    sigreap();

    /* ask processes to terminate */
    kill(-1, SIGTERM);
    sigreap();

    /* after SIGKILLTIMEOUT seconds, kill all processes */
    sleep(SIGKILLTIMEOUT);
    sigfpoweroff();
}

/**
 * this powers off the system with force level 1
 * it kills all processes, syncs filesystems,
 * runs the shutdown rcfile and then powers off the system.
**/
static void sigfpoweroff(void) {
    /* kill all processes */
    kill(-1, SIGKILL);
    sigreap();

    /* sync filesystems */
    sync();

    /* run rc.shutdown */
    spawnwait(rcshutdownfile);
    sigreap();

    /* power off system */
    sigffpoweroff();
}

/**
 * this powers off the system with force level 2
 * it directly powers off the system.
**/
static void sigffpoweroff(void) {
    /* power off system */
    if (vfork() == 0) {
        reboot(RB_POWER_OFF);
        _exit(EXIT_SUCCESS);
    }
    while (1)
        sleep(1);
}

/**
 * this reaps child processes
**/
static void sigreap(void) {
    /* reap children */
    while (waitpid(-1, NULL, WNOHANG) > 0);
    alarm(TIMEO);
}

/**
 * this safely reboots the system.
 * it stops services, asks processes to terminate,
 * kills non-terminating processes, syncs filesystems,
 * runs shutdown rcfile and then reboots the system.
**/
static void sigreboot(void) {
    /* stop services */
    spawnwait(servstopcmd);
    sigreap();

    /* ask processes to terminate */
    kill(-1, SIGTERM);
    sigreap();

    /* after SIGKILLTIMEOUT seconds, kill all processes */
    sleep(SIGKILLTIMEOUT);
    sigfreboot();
}

/**
 * this reboots the system with force level 1
 * it kills all processes, syncs filesystems,
 * runs the shutdown rcfile and then reboots the system.
**/
static void sigfreboot(void) {
    /* kill all processes */
    kill(-1, SIGKILL);
    sigreap();

    /* sync filesystems */
    sync();

    /* run rc.shutdown */
    spawnwait(rcshutdownfile);
    sigreap();

    /* reboot system */
    sigffreboot();
}

/**
 * this reboots the system with force level 2
 * it directly reboots the system.
**/
static void sigffreboot(void) {
    /* reboot system */
    if (vfork() == 0) {
        reboot(RB_AUTOBOOT);
        _exit(EXIT_SUCCESS);
    }
    while (1)
        sleep(1);
}

/**
 * this halts the system.
 * it performs the steps of a normal shutdown
 * and then tells the kernel to halt.
**/
static void sighalt(void) {
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

    /* halt system */
    sigfhalt();
}

/**
 * this halts the system with force level 2
 * it directly tells the kernel to halt the system.
**/
static void sigfhalt(void) {
    /* halt system */
    if (vfork() == 0) {
        reboot(RB_HALT_SYSTEM);
        _exit(EXIT_SUCCESS);
    }
    while (1)
        sleep(1);
}

/**
 * this hibernates the system
 * it tells the kernel to hibernate the system.
 * yet untested, probably not ready
 * for use in production (yet).
**/
static void sighibernate(void) {
    /* hibernate system */
    if (vfork() == 0) {
        reboot(RB_SW_SUSPEND);
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
            break;
        default:
            waitpid(chpid, NULL, WUNTRACED);
    }
}
