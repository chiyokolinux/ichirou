/**
 * this file is part of ichirou
 * Copyright (c) 2020-2021 Emily <elishikawa@jagudev.net>
 *
 * ichirou is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ichirou is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ichirou.  If not, see <https://www.gnu.org/licenses/>.
**/

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
