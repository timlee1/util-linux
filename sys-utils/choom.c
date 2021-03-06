/*
 * choom - Change OOM score setting
 *
 * Copyright (C) 2018 Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "nls.h"
#include "c.h"
#include "path.h"
#include "strutils.h"
#include "closestream.h"

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %1$s [options] -p pid\n"
		" %1$s [options] -n number -p pid\n"
		" %1$s [options] -n number command [args...]]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Display and adjust OOM-killer score.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -n, --adjust <num>     specify the adjust score value\n"), out);
	fputs(_(" -p, --pid <num>        process ID\n"), out);
	fputs(USAGE_SEPARATOR, out);
	printf(USAGE_HELP_OPTIONS(24));
	printf(USAGE_MAN_TAIL("choom(1)"));
	exit(EXIT_SUCCESS);
}

static int get_score(const pid_t pid)
{
	return path_read_s32("/proc/%d/oom_score", (int) pid);
}

static int get_score_adj(const pid_t pid)
{
	return path_read_s32("/proc/%d/oom_score_adj", (int) pid);
}

static int set_score_adj(const pid_t pid, int adj)
{
	char buf[sizeof(stringify_value(INT_MAX))];

	snprintf(buf, sizeof(buf), "%d", adj);

	if (path_write_str(buf, "/proc/%d/oom_score_adj", (int) pid) < 0)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	pid_t pid = 0;
	int c, adj = 0, has_adj = 0;

	static const struct option longopts[] = {
		{ "adjust",  required_argument, NULL, 'n' },
		{ "pid",     required_argument, NULL, 'p' },
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'V' },
		{ NULL,      0,                 NULL,  0  }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((c = getopt_long(argc, argv, "hn:p:V", longopts, NULL)) != -1) {
		switch (c) {
		case 'p':
			pid = strtos32_or_err(optarg, _("invalid PID argument"));
			break;
		case 'n':
			adj = strtos32_or_err(optarg, _("invalid adjust argument"));
			has_adj = 1;
			break;
		case 'V':
			printf(UTIL_LINUX_VERSION);
			return EXIT_SUCCESS;
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind < argc && pid) {
		warnx(_("invalid argument: %s"), argv[optind]);
		errtryhelp(EXIT_FAILURE);
	}
	if (!pid && argc - optind < 1) {
		warnx(_("no PID or COMMAND specified"));
		errtryhelp(EXIT_FAILURE);
	}
	if (optind < argc && !has_adj) {
		warnx(_("no OOM score adjust value specified"));
		errtryhelp(EXIT_FAILURE);
	}

	/* Show */
	if (!has_adj) {
		printf(_("pid %d's current OOM score: %d\n"), pid, get_score(pid));
		printf(_("pid %d's current OOM score adjust value: %d\n"), pid, get_score_adj(pid));

	/* Change */
	} else if (pid) {
		int old = get_score_adj(pid);

		if (set_score_adj(pid, adj))
			err(EXIT_FAILURE, _("failed to set score adjust value"));

		printf(_("pid %d's OOM score adjust value changed from %d to %d\n"), pid, old, adj);

	/* Start new process */
	} else {
		if (set_score_adj(getpid(), adj))
			err(EXIT_FAILURE, _("failed to set score adjust value"));
		argv += optind;
		execvp(argv[0], argv);
		errexec(argv[0]);
	}

	return EXIT_SUCCESS;
}
