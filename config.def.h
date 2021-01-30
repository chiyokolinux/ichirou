static char *const rcinitfile[]     = { "/bin/rc.init",             NULL };
static char *const rcpostinitfile[] = { "/bin/rc.postinit",         NULL };
static char *const servstartcmd[]   = { "/sbin/kanrisha", "daemon", NULL };
static char *const rcshutdownfile[] = { "/bin/rc.shutdown",         NULL };
static char *const servstopcmd[]    = { "/sbin/kanrisha", "stop",   NULL };

#define SIGKILLTIMEOUT  10
#define MAXSERVICES     512
#define MAXSVCRESTART   128

#define LOGFILEPERMS    0600
#define STATUSLOGLEN    "8"

#define WRITE_TO_SYSLOG
#define WRITE_TO_OUTPUT

#define CMDFIFOPATH     "/tmp/kanrisha.cmd.sock"
