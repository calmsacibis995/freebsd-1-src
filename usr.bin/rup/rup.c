/*-
 * Copyright (c) 1993, John Brezak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$Id: rup.c,v 1.2 1993/09/23 18:45:57 jtc Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <arpa/inet.h>

#undef FSHIFT			/* Use protocol's shift and scale values */
#undef FSCALE
#include <rpcsvc/rstat.h>

#define HOST_WIDTH 15

char *argv0;

struct host_list {
	struct host_list *next;
	struct in_addr addr;
} *hosts;

int search_host(struct in_addr addr)
{
	struct host_list *hp;
	
	if (!hosts)
		return(0);

	for (hp = hosts; hp != NULL; hp = hp->next) {
		if (hp->addr.s_addr == addr.s_addr)
			return(1);
	}
	return(0);
}

void remember_host(struct in_addr addr)
{
	struct host_list *hp;

	if (!(hp = (struct host_list *)malloc(sizeof(struct host_list)))) {
		fprintf(stderr, "%s: no memory.\n", argv0);
		exit(1);
	}
	hp->addr.s_addr = addr.s_addr;
	hp->next = hosts;
	hosts = hp;
}

rstat_reply(char *replyp, struct sockaddr_in *raddrp)
{
	struct tm *tmp_time;
	struct tm host_time;
	struct tm host_uptime;
	char days_buf[16];
	char hours_buf[16];
	struct hostent *hp;
	char *host;
	statstime *host_stat = (statstime *)replyp;

	if (search_host(raddrp->sin_addr))
		return(0);
	
	hp = gethostbyaddr((char *)&raddrp->sin_addr.s_addr,
			   sizeof(struct in_addr), AF_INET);
	if (hp)
		host = hp->h_name;
	else
		host = inet_ntoa(raddrp->sin_addr);

	printf("%-*s\t", HOST_WIDTH, host);

	tmp_time = localtime((time_t *)&host_stat->curtime.tv_sec);
	host_time = *tmp_time;

	host_stat->curtime.tv_sec -= host_stat->boottime.tv_sec;

	tmp_time = gmtime((time_t *)&host_stat->curtime.tv_sec);
	host_uptime = *tmp_time;

	if (host_uptime.tm_yday != 0)
		sprintf(days_buf, "%3d day%s, ", host_uptime.tm_yday,
			(host_uptime.tm_yday > 1) ? "s" : "");
	else
		days_buf[0] = '\0';

	if (host_uptime.tm_hour != 0)
		sprintf(hours_buf, "%2d:%02d, ",
			host_uptime.tm_hour, host_uptime.tm_min);
	else
		if (host_uptime.tm_min != 0)
			sprintf(hours_buf, "%2d mins, ", host_uptime.tm_min);
		else
			hours_buf[0] = '\0';

	printf(" %2d:%02d%cm  up %9.9s%9.9s load average: %.2f %.2f %.2f\n",
		host_time.tm_hour % 12,
		host_time.tm_min,
		(host_time.tm_hour >= 12) ? 'p' : 'a',
		days_buf,
		hours_buf,
		(double)host_stat->avenrun[0]/FSCALE,
		(double)host_stat->avenrun[1]/FSCALE,
		(double)host_stat->avenrun[2]/FSCALE);

	remember_host(raddrp->sin_addr);
	return(0);
}

onehost(char *host)
{
	CLIENT *rstat_clnt;
	statstime host_stat;
	struct sockaddr_in addr;
	struct hostent *hp;
	
	hp = gethostbyname(host);
	if (hp == NULL) {
		fprintf(stderr, "%s: unknown host \"%s\"\n",
			argv0, host);
		return(-1);
	}

	rstat_clnt = clnt_create(host, RSTATPROG, RSTATVERS_TIME, "udp");
	if (rstat_clnt == NULL) {
		fprintf(stderr, "%s: %s %s", argv0, host, clnt_spcreateerror(""));
		return(-1);
	}

	bzero((char *)&host_stat, sizeof(host_stat));
	if (clnt_call(rstat_clnt, RSTATPROC_STATS, xdr_void, NULL, xdr_statstime, &host_stat, NULL) != RPC_SUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", argv0, host, clnt_sperror(rstat_clnt, host));
		return(-1);
	}

	addr.sin_addr.s_addr = *(int *)hp->h_addr;
	rstat_reply((char *)&host_stat, &addr);
}

allhosts()
{
	statstime host_stat;
	enum clnt_stat clnt_stat;

	clnt_stat = clnt_broadcast(RSTATPROG, RSTATVERS_TIME, RSTATPROC_STATS,
				   xdr_void, NULL,
				   xdr_statstime, &host_stat, rstat_reply);
	if (clnt_stat != RPC_SUCCESS && clnt_stat != RPC_TIMEDOUT) {
		fprintf(stderr, "%s: %s\n", argv0, clnt_sperrno(clnt_stat));
		exit(1);
	}
}

usage()
{
	fprintf(stderr, "Usage: %s [hosts ...]\n", argv0);
	exit(1);
}

main(int argc, char *argv[])
{
	int ch;
	extern int optind;

	if (!(argv0 = rindex(argv[0], '/')))
		argv0 = argv[0];
	else
		argv0++;
    
	while ((ch = getopt(argc, argv, "?")) != -1)
		switch (ch) {
		default:
			usage();
			/*NOTREACHED*/
		}
	
	setlinebuf(stdout);
	if (argc == optind)
		allhosts();
	else {
		for (; optind < argc; optind++)
			(void) onehost(argv[optind]);
	}
	exit(0);
}
