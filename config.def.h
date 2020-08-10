static char *const rcinitfile[]     = { "/bin/rc.init",            NULL };
static char *const rcpostinitfile[] = { "/bin/rc.postinit",        NULL };
static char *const servstartcmd[]   = { "/bin/kanrisha", "start",  NULL };
static char *const rcshutdownfile[] = { "/bin/rc.shutdown",        NULL };
static char *const servstopcmd[]    = { "/bin/kanrisha", "stop",   NULL };

#define SIGKILLTIMEOUT  10
#define MAXSERVICES     512
