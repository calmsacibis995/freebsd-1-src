/*
 * man.c
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.  
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 */

#define MAN_MAIN

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/file.h>
#include <signal.h>
#include "config.h"
#include "gripes.h"
#include "version.h"

#ifndef POSIX
#include <unistd.h>
#else
#ifndef R_OK
#define R_OK 4
#endif
#endif

#ifdef SECURE_MAN_UID
extern uid_t getuid ();
extern int setuid ();
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern char *malloc ();
extern char *getenv ();
extern void free ();
extern int system ();
extern int strcmp ();
extern int strncmp ();
extern int exit ();
extern int fflush ();
extern int printf ();
extern int fprintf ();
extern FILE *fopen ();
extern int fclose ();
extern char *sprintf ();
#endif

extern char *strdup ();

extern char **glob_vector ();
extern char **glob_filename ();
extern int access ();
extern int unlink ();
extern int system ();
extern int chmod ();
extern int is_newer ();
extern int is_directory ();
extern int do_system_command ();

char *prognam;
static char *pager;
static char *manp;
static char *manpathlist[MAXDIRS];
static char *section;
static char *colon_sep_section_list;
static char **section_list;
static char *roff_directive;
static int apropos;
static int whatis;
static int findall;
static int print_where;

#ifdef ALT_SYSTEMS
static int alt_system;
static char *alt_system_name;
#endif

static int troff = 0;

int debug;

#ifdef HAS_TROFF
#ifdef ALT_SYSTEMS
static char args[] = "M:P:S:adfhkm:p:tw?";
#else
static char args[] = "M:P:S:adfhkp:tw?";
#endif
#else
#ifdef ALT_SYSTEMS
static char args[] = "M:P:S:adfhkm:p:w?";
#else
static char args[] = "M:P:S:adfhkp:w?";
#endif
#endif

#ifdef SETREUID
uid_t ruid;
uid_t euid;
uid_t rgid;
uid_t egid;
#endif

int
main (argc, argv)
     int argc;
     char **argv;
{
  int status = 0;
  char *nextarg;
  char *tmp;
  extern int optind;
  extern char *mkprogname ();
  char *is_section ();
  char **get_section_list ();
  void man_getopt ();
  void do_apropos ();
  void do_whatis ();
  int man ();

  prognam = mkprogname (argv[0]);

  man_getopt (argc, argv);

  if (optind == argc)
    gripe_no_name ((char *)NULL);

  section_list = get_section_list ();

  if (optind == argc - 1)
    {
      tmp = is_section (argv[optind]);

      if (tmp != NULL)
	gripe_no_name (tmp);
    }

#ifdef SETREUID
  ruid = getuid();
  rgid = getgid();
  euid = geteuid();
  egid = getegid();
  setreuid(-1, ruid);
  setregid(-1, rgid);
#endif

  while (optind < argc)
    {
      nextarg = argv[optind++];

      /*
       * See if this argument is a valid section name.  If not,
       * is_section returns NULL.
       */
      tmp = is_section (nextarg);

      if (tmp != NULL)
	{
	  section = tmp;

	  if (debug)
	    fprintf (stderr, "\nsection: %s\n", section);

	  continue;
	}

      if (apropos)
	do_apropos (nextarg);
      else if (whatis)
	do_whatis (nextarg);
      else
	{
	  status = man (nextarg);

	  if (status == 0)
	    gripe_not_found (nextarg, section);
	}
    }
  return status;
}

void
usage ()
{
  static char usage_string[1024] = "%s, version %s\n\n";

#ifdef HAS_TROFF
#ifdef ALT_SYSTEMS
  static char s1[] =
    "usage: %s [-adfhktw] [section] [-M path] [-P pager] [-S list]\n\
           [-m system] [-p string] name ...\n\n";
#else
  static char s1[] =
    "usage: %s [-adfhktw] [section] [-M path] [-P pager] [-S list]\n\
           [-p string] name ...\n\n";
#endif
#else
#ifdef ALT_SYSTEMS
  static char s1[] =
    "usage: %s [-adfhkw] [section] [-M path] [-P pager] [-S list]\n\
           [-m system] [-p string] name ...\n\n";
#else
  static char s1[] =
    "usage: %s [-adfhkw] [section] [-M path] [-P pager] [-S list]\n\
           [-p string] name ...\n\n";
#endif
#endif

static char s2[] = "  a : find all matching entries\n\
  d : print gobs of debugging information\n\
  f : same as whatis(1)\n\
  h : print this help message\n\
  k : same as apropos(1)\n";

#ifdef HAS_TROFF
  static char s3[] = "  t : use troff to format pages for printing\n";
#endif

  static char s4[] = "  w : print location of man page(s) that would be displayed\n\n\
  M path   : set search path for manual pages to `path'\n\
  P pager  : use program `pager' to display pages\n\
  S list   : colon separated section list\n";

#ifdef ALT_SYSTEMS
  static char s5[] = "  m system : search for alternate system's man pages\n";
#endif

  static char s6[] = "  p string : string tells which preprocessors to run\n\
               e - [n]eqn(1)   p - pic(1)    t - tbl(1)\n\
               g - grap(1)     r - refer(1)  v - vgrind(1)\n";

  strcat (usage_string, s1);
  strcat (usage_string, s2);

#ifdef HAS_TROFF
  strcat (usage_string, s3);
#endif

  strcat (usage_string, s4);

#ifdef ALT_SYSTEMS
  strcat (usage_string, s5);
#endif

  strcat (usage_string, s6);

  fprintf (stderr, usage_string, prognam, version, prognam);
  exit(1);
}

char **
add_dir_to_mpath_list (mp, p)
     char **mp;
     char *p;
{
  int status;

  status = is_directory (p);

  if (status < 0)
    {
      fprintf (stderr, "Warning: couldn't stat file %s!\n", p);
    }
  else if (status == 0)
    {
      fprintf (stderr, "Warning: %s isn't a directory!\n", p);
    }
  else if (status == 1)
    {
      if (debug)
	fprintf (stderr, "adding %s to manpathlist\n", p);

      *mp++ = strdup (p);
    }
  return mp;
}

/*
 * Get options from the command line and user environment.
 */
void
man_getopt (argc, argv)
     register int argc;
     register char **argv;
{
  register int c;
  register char *p;
  register char *end;
  register char **mp;
  extern char *optarg;
  extern int getopt ();
  extern void downcase ();
  extern char *manpath ();

  while ((c = getopt (argc, argv, args)) != EOF)
    {
      switch (c)
	{
	case 'M':
	  manp = strdup (optarg);
	  break;
	case 'P':
	  pager = strdup (optarg);
	  break;
	case 'S':
	  colon_sep_section_list = strdup (optarg); 
	  break;
	case 'a':
	  findall++;
	  break;
	case 'd':
	  debug++;
	  break;
	case 'f':
	  if (troff)
	    gripe_incompatible ("-f and -t");
	  if (apropos)
	    gripe_incompatible ("-f and -k");
	  if (print_where)
	    gripe_incompatible ("-f and -w");
	  whatis++;
	  break;
	case 'k':
	  if (troff)
	    gripe_incompatible ("-k and -t");
	  if (whatis)
	    gripe_incompatible ("-k and -f");
	  if (print_where)
	    gripe_incompatible ("-k and -w");
	  apropos++;
	  break;
#ifdef ALT_SYSTEMS
	case 'm':
	  alt_system++;
	  alt_system_name = strdup (optarg);
	  break;
#endif
	case 'p':
	  roff_directive = strdup (optarg);
	  break;
#ifdef HAS_TROFF
	case 't':
	  if (apropos)
	    gripe_incompatible ("-t and -k");
	  if (whatis)
	    gripe_incompatible ("-t and -f");
	  if (print_where)
	    gripe_incompatible ("-t and -w");
	  troff++;
	  break;
#endif
	case 'w':
	  if (apropos)
	    gripe_incompatible ("-w and -k");
	  if (whatis)
	    gripe_incompatible ("-w and -f");
	  if (troff)
	    gripe_incompatible ("-w and -t");
	  print_where++;
	  break;
	case 'h':
	case '?':
	default:
	  usage();
	  break;
	}
    }

  if (pager == NULL || *pager == '\0')
    if ((pager = getenv ("PAGER")) == NULL)
      pager = strdup (PAGER);

  if (debug)
    fprintf (stderr, "\nusing %s as pager\n", pager);

  if (manp == NULL)
    {
      if ((manp = manpath (0)) == NULL)
	gripe_manpath ();

      if (debug)
	fprintf (stderr,
		 "\nsearch path for pages determined by manpath is\n%s\n\n",
		 manp);
    }

#ifdef ALT_SYSTEMS
  if (alt_system_name == NULL || *alt_system_name == '\0')
    if ((alt_system_name = getenv ("SYSTEM")) != NULL)
      alt_system_name = strdup (alt_system_name);

  if (alt_system_name != NULL && *alt_system_name != '\0')
    downcase (alt_system_name);
#endif

  /*
   * Expand the manpath into a list for easier handling.
   */
  mp = manpathlist;
  for (p = manp; ; p = end+1)
    {
      if ((end = strchr (p, ':')) != NULL)
	*end = '\0';

#ifdef ALT_SYSTEMS
      if (alt_system)
	{
	  char buf[FILENAME_MAX];

	  if (debug)
	    fprintf (stderr, "Alternate system `%s' specified\n",
		     alt_system_name);

	  strcpy (buf, p);
	  strcat (buf, "/");
	  strcat (buf, alt_system_name);

	  mp = add_dir_to_mpath_list (mp, buf);
	}
      else
	{
	  mp = add_dir_to_mpath_list (mp, p);
	}
#else
      mp = add_dir_to_mpath_list (mp, p);
#endif
      if (end == NULL)
	break;

      *end = ':';
    }
  *mp = NULL;
}

/*
 * Check to see if the argument is a valid section number.  If the
 * first character of name is a numeral, or the name matches one of
 * the sections listed in section_list, we'll assume that it's a section.
 * The list of sections in config.h simply allows us to specify oddly
 * named directories like .../man3f.  Yuk. 
 */
char *
is_section (name)
     register char *name;
{
  register char **vs;

  for (vs = section_list; *vs != NULL; vs++)
    if ((strcmp (*vs, name) == NULL) || (isdigit (name[0])))
      return strdup (name);

  return NULL;
}

/*
 * Handle the apropos option.  Cheat by using another program.
 */
void
do_apropos (name)
     register char *name;
{
  register int len;
  register char *command;

  len = strlen (APROPOS) + strlen (name) + 2;

  if ((command = (char *) malloc(len)) == NULL)
    gripe_alloc (len, "command");

  sprintf (command, "%s %s", APROPOS, name);

  (void) do_system_command (command);

  free (command);
}

/*
 * Handle the whatis option.  Cheat by using another program.
 */
void
do_whatis (name)
     register char *name;
{
  register int len;
  register char *command;

  len = strlen (WHATIS) + strlen (name) + 2;

  if ((command = (char *) malloc(len)) == NULL)
    gripe_alloc (len, "command");

  sprintf (command, "%s %s", WHATIS, name);

  (void) do_system_command (command);

  free (command);
}

/*
 * Change a name of the form ...man/man1/name.1 to ...man/cat1/name.1
 * or a name of the form ...man/cat1/name.1 to ...man/man1/name.1
 */
char *
convert_name (name, to_cat)
     register char *name;
     register int to_cat;
{
  register char *to_name;
  register char *t1;
  register char *t2 = NULL;

#ifdef DO_COMPRESS
  if (to_cat)
    {
      int len = strlen (name) + 3;
      to_name = (char *) malloc (len);
      if (to_name == NULL)
	gripe_alloc (len, "to_name");
      strcpy (to_name, name);
      strcat (to_name, ".Z");
    }
  else
    to_name = strdup (name);
#else
  to_name = strdup (name);
#endif

  t1 = strrchr (to_name, '/');
  if (t1 != NULL)
    {
      *t1 = NULL;
      t2 = strrchr (to_name, '/');
      *t1 = '/';
    }

  if (t2 == NULL)
    gripe_converting_name (name, to_cat);

  if (to_cat)
    {
      *(++t2) = 'c';
      *(t2+2) = 't';
    }
  else
    {
      *(++t2) = 'm';
      *(t2+2) = 'n';
    }

  if (debug)
    fprintf (stderr, "to_name in convert_name () is: %s\n", to_name);

  return to_name;
}

/*
 * Try to find the man page corresponding to the given name.  The
 * reason we do this with globbing is because some systems have man
 * page directories named man3 which contain files with names like
 * XtPopup.3Xt.  Rather than requiring that this program know about
 * all those possible names, we simply try to match things like
 * .../man[sect]/name[sect]*.  This is *much* easier.
 *
 * Note that globbing is only done when the section is unspecified.
 */
char **
glob_for_file (path, section, name, cat)
     register char *path;
     register char *section;
     register char *name;
     register int cat;
{
  char pathname[FILENAME_MAX];
  char **gf;

  if (cat)
    sprintf (pathname, "%s/cat%s/%s.%s*", path, section, name, section);
  else
    sprintf (pathname, "%s/man%s/%s.%s*", path, section, name, section);

  if (debug)
    fprintf (stderr, "globbing %s\n", pathname);

  gf = glob_filename (pathname);

  if ((gf == (char **) -1 || *gf == NULL) && isdigit (*section))
    {
      if (cat)
	sprintf (pathname, "%s/cat%s/%s.%c*", path, section, name, *section);
      else
	sprintf (pathname, "%s/man%s/%s.%c*", path, section, name, *section);

      gf = glob_filename (pathname);
    }
  if ((gf == (char **) -1 || *gf == NULL) && isdigit (*section))
    {
      if (cat)
	sprintf (pathname, "%s/cat%s/%s.0*", path, section, name);
      else
	sprintf (pathname, "%s/man%s/%s.0*", path, section, name);
      if (debug)
	fprintf (stderr, "globbing %s\n", pathname);
      gf = glob_filename (pathname);
    }
  return gf;
}

/*
 * Return an un-globbed name in the same form as if we were doing
 * globbing. 
 */
char **
make_name (path, section, name, cat)
     register char *path;
     register char *section;
     register char *name;
     register int cat;
{
  register int i = 0;
  static char *names[3];
  char buf[FILENAME_MAX];

  if (cat)
    sprintf (buf, "%s/cat%s/%s.%s", path, section, name, section);
  else
    sprintf (buf, "%s/man%s/%s.%s", path, section, name, section);

  if (access (buf, R_OK) == 0)
    names[i++] = strdup (buf);

  /*
   * If we're given a section that looks like `3f', we may want to try
   * file names like .../man3/foo.3f as well.  This seems a bit
   * kludgey to me, but what the hey...
   */
  if (section[1] != '\0')
    {
      if (cat)
	sprintf (buf, "%s/cat%c/%s.%s", path, section[0], name, section);
      else
	sprintf (buf, "%s/man%c/%s.%s", path, section[0], name, section);

      if (access (buf, R_OK) == 0)
	names[i++] = strdup (buf);
    }

  names[i] = NULL;

  return &names[0];
}

#ifdef DO_UNCOMPRESS
char *
get_expander (file)
     char *file;
{
  char *expander = NULL;
  int len = strlen (file);

  if (file[len - 2] == '.')
    {
      switch (file[len - 1])
	{
#ifdef FCAT
	case 'F':
	  if (strcmp (FCAT, "") != 0)
	    expander = strdup (FCAT);
	  break;
#endif
#ifdef YCAT
	case 'Y':
	  if (strcmp (YCAT, "") != 0)
	    expander = strdup (YCAT);
	  break;
#endif
#ifdef ZCAT
	case 'Z':
	  if (strcmp (ZCAT, "") != 0)
	    expander = strdup (ZCAT);
	  break;
#endif
	default:
	  break;
	}
    }
  return expander;
}
#endif

/*
 * Simply display the preformatted page.
 */
int
display_cat_file (file)
     register char *file;
{
  register int found;
  char command[FILENAME_MAX];

  found = 0;

  if (access (file, R_OK) == 0)
    {
#ifdef DO_UNCOMPRESS
      char *expander = get_expander (file);

      if (expander != NULL)
	sprintf (command, "%s %s | %s", expander, file, pager);
      else
	sprintf (command, "%s %s", pager, file);
#else
      sprintf (command, "%s %s", pager, file);
#endif

      found = do_system_command (command);
    }
  return found;
}

/*
 * Try to find the ultimate source file.  If the first line of the
 * current file is not of the form
 *
 *      .so man3/printf.3s
 *
 * the input file name is returned.
 */
char *
ultimate_source (name, path)
     char *name;
     char *path;
{
  static  char buf[BUFSIZ];
  static  char ult[FILENAME_MAX];

  FILE *fp;
  char *beg;
  char *end;

  strcpy (ult, name);
  strcpy (buf, name);

 next:

  if ((fp = fopen (ult, "r")) == NULL)
    return buf;

  if (fgets (buf, BUFSIZ, fp) == NULL)
    return ult;

  if (strlen (buf) < 5)
    return ult;

  beg = buf;
  if (*beg++ == '.' && *beg++ == 's' && *beg++ == 'o')
    {
      while ((*beg == ' ' || *beg == '\t') && *beg != '\0')
	beg++;

      end = beg;
      while (*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0')
	end++;

      *end = '\0';

      strcpy (ult, path);
      strcat (ult, "/");
      strcat (ult, beg);

      strcpy (buf, ult);

      goto next;
    }

  if (debug)
    fprintf (stderr, "found ultimate source file %s\n", ult);

  return ult;
}

void
add_directive (first, d, file, buf)
     int *first;
     char *d;
     char *file;
     char *buf;
{
  if (strcmp (d, "") != 0)
    {
      if (*first)
	{
	  *first = 0;
	  strcpy (buf, d);
	  strcat (buf, " ");
	  strcat (buf, file);
	}
      else
	{
	  strcat (buf, " | ");
	  strcat (buf, d);
	}
    }
}

int
parse_roff_directive (cp, file, buf)
  char *cp;
  char *file;
  char *buf;
{
  char c;
  int first = 1;
  int tbl_found = 0;

  while ((c = *cp++) != '\0')
    {
      switch (c)
	{
	case 'e':

	  if (debug)
	    fprintf (stderr, "found eqn(1) directive\n");

	  if (troff)
	    add_directive (&first, EQN, file, buf);
	  else
	    add_directive (&first, NEQN, file, buf);

	  break;

	case 'g':

	  if (debug)
	    fprintf (stderr, "found grap(1) directive\n");

	  add_directive (&first, GRAP, file, buf);

	  break;

	case 'p':

	  if (debug)
	    fprintf (stderr, "found pic(1) directive\n");

	  add_directive (&first, PIC, file, buf);

	  break;

	case 't':

	  if (debug)
	    fprintf (stderr, "found tbl(1) directive\n");

	  tbl_found++;
	  add_directive (&first, TBL, file, buf);
	  break;

	case 'v':

	  if (debug)
	    fprintf (stderr, "found vgrind(1) directive\n");

	  add_directive (&first, VGRIND, file, buf);
	  break;

	case 'r':

	  if (debug)
	    fprintf (stderr, "found refer(1) directive\n");

	  add_directive (&first, REFER, file, buf);
	  break;

	case ' ':
	case '\t':
	case '\n':

	  goto done;

	default:

	  return -1;
	}
    }

 done:

  if (first)
    return 1;

#ifdef HAS_TROFF
  if (troff)
    {
      strcat (buf, " | ");
      strcat (buf, TROFF);
    }
  else
#endif
    {
      strcat (buf, " | ");
      strcat (buf, NROFF);
    }

  if (tbl_found && !troff && strcmp (COL, "") != 0)
    {
      strcat (buf, " | ");
      strcat (buf, COL);
    }

  return 0;
}

char *
make_roff_command (file)
     char *file;
{
  FILE *fp;
  char line [BUFSIZ];
  static char buf [BUFSIZ];
  int status;
  char *cp;

  if (roff_directive != NULL)
    {
      if (debug)
	fprintf (stderr, "parsing directive from command line\n");

      status = parse_roff_directive (roff_directive, file, buf);

      if (status == 0)
	return buf;

      if (status == -1)
	gripe_roff_command_from_command_line (file);
    }

  if ((fp = fopen (file, "r")) != NULL)
    {
      cp = line;
      fgets (line, 100, fp);
      if (*cp++ == '\'' && *cp++ == '\\' && *cp++ == '"' && *cp++ == ' ')
	{
	  if (debug)
	    fprintf (stderr, "parsing directive from file\n");

	  status = parse_roff_directive (cp, file, buf);

	  fclose (fp);

	  if (status == 0)
	    return buf;

	  if (status == -1)
	    gripe_roff_command_from_file (file);
	}
    }
  else
    {
      /*
       * Is there really any point in continuing to look for
       * preprocessor options if we can't even read the man page source? 
       */
      gripe_reading_man_file (file);
      return NULL;
    }

  if ((cp = getenv ("MANROFFSEQ")) != NULL)
    {
      if (debug)
	fprintf (stderr, "parsing directive from environment\n");

      status = parse_roff_directive (cp, file, buf);

      if (status == 0)
	return buf;

      if (status == -1)
	gripe_roff_command_from_env ();
    }

  if (debug)
    fprintf (stderr, "using default preprocessor sequence\n");

#ifdef HAS_TROFF
  if (troff)
    {
      if (strcmp (TBL, "") != 0)
	{
	  strcpy (buf, TBL);
	  strcat (buf, " ");
	  strcat (buf, file);
	  strcat (buf, " | ");
	  strcat (buf, TROFF);
	}
      else
	{
	  strcpy (buf, TROFF);
	  strcat (buf, " ");
	  strcat (buf, file);
	}
    }
  else
#endif
    {
      if (strcmp (TBL, "") != 0)
	{
	  strcpy (buf, TBL);
	  strcat (buf, " ");
	  strcat (buf, file);
	  strcat (buf, " | ");
	  strcat (buf, NROFF);
	}
      else
	{
	  strcpy (buf, NROFF);
	  strcat (buf, " ");
	  strcat (buf, file);
	}

      if (strcmp (COL, "") != 0)
	{
	  strcat (buf, " | ");
	  strcat (buf, COL);
	}
    }
  return buf;
}

/*
 * Try to format the man page and create a new formatted file.  Return
 * 1 for success and 0 for failure.
 */
int
make_cat_file (path, man_file, cat_file)
     register char *path;
     register char *man_file;
     register char *cat_file;
{
  int status;
  int mode;
  FILE *fp;
  char *roff_command;
  char command[FILENAME_MAX];
  char temp[FILENAME_MAX];

  sprintf(temp, "%s.tmp", cat_file);
  if ((fp = fopen (temp, "w")) != NULL)
    {
      fclose (fp);
      unlink (temp);

      roff_command = make_roff_command (man_file);
      if (roff_command == NULL)
	return 0;
      else
#ifdef DO_COMPRESS
	sprintf (command, "(cd %s ; %s | %s > %s)", path,
		 roff_command, COMPRESSOR, temp);
#else
        sprintf (command, "(cd %s ; %s > %s)", path,
		 roff_command, temp);
#endif
      /*
       * Don't let the user interrupt the system () call and screw up
       * the formmatted man page if we're not done yet.
       */
      fprintf (stderr, "Formatting page, please wait...");
      fflush(stderr);

      status = do_system_command (command);

      if (!status) {
	fprintf(stderr, "Failed.\n");
	unlink(temp);
	exit(1);
      }
      else {
        if (rename(temp, cat_file) == -1) {
		/* FS might be sticky */
		sprintf(command, "cp %s %s", temp, cat_file);
		if (system(command))
			fprintf(stderr,
				"\nHmm!  Can't seem to rename %s to %s, check permissions on man dir!\n",
				temp, cat_file);
		unlink(temp);
		return 0;
	}
      }
      fprintf(stderr, "Done.\n");
      if (status == 1)
	{
	  mode = CATMODE;
	  chmod (cat_file, mode);

	  if (debug)
	    fprintf (stderr, "mode of %s is now %o\n", cat_file, mode);
	}


      return 1;
    }
  else
    {
      if (debug)
	fprintf (stderr, "Couldn't open %s for writing.\n", cat_file);

      return 0;
    }
}

/*
 * Try to format the man page source and save it, then display it.  If
 * that's not possible, try to format the man page source and display
 * it directly.
 *
 * Note that we've already been handed the name of the ultimate source
 * file at this point.
 */
int
format_and_display (path, man_file, cat_file)
     register char *path;
     register char *man_file;
     register char *cat_file;
{
  int status;
  register int found;
  char *roff_command;
  char command[FILENAME_MAX];

  found = 0;

  if (access (man_file, R_OK) != 0)
    return 0;
  
  if (troff)
    {
      roff_command = make_roff_command (man_file);
      if (roff_command == NULL)
	return 0;
      else
	sprintf (command, "(cd %s ; %s)", path, roff_command);

      found = do_system_command (command);
    }
  else
    {
      status = is_newer (man_file, cat_file);
      if (debug)
	fprintf (stderr, "status from is_newer() = %d\n");

      if (status == 1 || status == -2)
	{
	  /*
	   * Cat file is out of date.  Try to format and save it.
	   */
	  if (print_where)
	    {
	      printf ("%s\n", man_file);
	      found++;
	    }
	  else
	    {

#ifdef SETREUID
	      setreuid(-1, euid);
	      setregid(-1, egid);
#endif

	      found = make_cat_file (path, man_file, cat_file);

#ifdef SETREUID
	      setreuid(-1, ruid);
	      setregid(-1, rgid);

	      if (!found)
	        {
		  /* Try again as real user - see note below.
		     By running with 
		       effective group (user) ID == real group (user) ID
		     except for the call above, I believe the problems
		     of reading private man pages is avoided.  */
		  found = make_cat_file (path, man_file, cat_file);
	        }
#endif
#ifdef SECURE_MAN_UID
	      if (!found)
		{
		  /*
		   * Try again as real user.  Note that for private
		   * man pages, we won't even get this far unless the
		   * effective user can read the real user's man page
		   * source.  Also, if we are trying to find all the
		   * man pages, this will probably make it impossible
		   * to make cat files in the system directories if
		   * the real user's man directories are searched
		   * first, because there's no way to undo this (is
		   * there?).  Yikes, am I missing something obvious?
		   */
		  setuid (getuid ());

		  found = make_cat_file (path, man_file, cat_file);
		}
#endif
	      if (found)
		{
		  /*
		   * Creating the cat file worked.  Now just display it.
		   */
		  (void) display_cat_file (cat_file);
		}
	      else
		{
		  /*
		   * Couldn't create cat file.  Just format it and
		   * display it through the pager. 
		   */
		  roff_command = make_roff_command (man_file);
		  if (roff_command == NULL)
		    return 0;
		  else
		    sprintf (command, "(cd %s ; %s | %s)", path,
			     roff_command, pager);

		  found = do_system_command (command);
		}
	    }
	}
      else if (access (cat_file, R_OK) == 0)
	{
	  /*
	   * Formatting not necessary.  Cat file is newer than source
	   * file, or source file is not present but cat file is.
	   */
	  if (print_where)
	    {
	      printf ("%s (source: %s)\n", cat_file, man_file);
	      found++;
	    }
	  else
	    {
	      found = display_cat_file (cat_file);
	    }
	}
    }
  return found;
}

/*
 * See if the preformatted man page or the source exists in the given
 * section.
 */
int
try_section (path, section, name, glob)
     register char *path;
     register char *section;
     register char *name;
     register int glob;
{
  register int found = 0;
  register int to_cat;
  register int cat;
  register char **names;
  register char **np;

  if (debug)
    {
      if (glob)
	fprintf (stderr, "trying section %s with globbing\n", section);
      else
	fprintf (stderr, "trying section %s without globbing\n", section);
    }

#ifndef NROFF_MISSING
  /*
   * Look for man page source files.
   */
  cat = 0;
  if (glob)
    names = glob_for_file (path, section, name, cat);
  else
    names = make_name (path, section, name, cat);

  if (names == (char **) -1 || *names == NULL)
    /*
     * No files match.  See if there's a preformatted page around that
     * we can display. 
     */
#endif /* NROFF_MISSING */
    {
      if (!troff)
	{
	  cat = 1;
	  if (glob)
	    names = glob_for_file (path, section, name, cat);
	  else
	    names = make_name (path, section, name, cat);

	  if (names != (char **) -1 && *names != NULL)
	    {
	      for (np = names; *np != NULL; np++)
		{
		  if (print_where)
		    {
		      printf ("%s\n", *np);
		      found++;
		    }
		  else
		    {
		      found += display_cat_file (*np);
		    }
		}
	    }
	}
    }
#ifndef NROFF_MISSING
  else
    {
      for (np = names; *np != NULL; np++)
	{
	  register char *cat_file = NULL;
	  register char *man_file;

	  man_file = ultimate_source (*np, path);

	  if (!troff)
	    {
	      to_cat = 1;

	      cat_file = convert_name (man_file, to_cat);

	      if (debug)
		fprintf (stderr, "will try to write %s if needed\n", cat_file);
	    }

	  found += format_and_display (path, man_file, cat_file);
	}
    }
#endif /* NROFF_MISSING */
  return found;
}

/*
 * Search for manual pages.
 *
 * If preformatted manual pages are supported, look for the formatted
 * file first, then the man page source file.  If they both exist and
 * the man page source file is newer, or only the source file exists,
 * try to reformat it and write the results in the cat directory.  If
 * it is not possible to write the cat file, simply format and display
 * the man file.
 *
 * If preformatted pages are not supported, or the troff option is
 * being used, only look for the man page source file.
 *
 */
int
man (name)
     char *name;
{
  register int found;
  register int glob;
  register char **mp;
  register char **sp;

  found = 0;

  fflush (stdout);
  if (section != NULL)
    {
      for (mp = manpathlist; *mp != NULL; mp++)
	{
	  if (debug)
	    fprintf (stderr, "\nsearching in %s\n", *mp);

	  glob = 0;

	  found += try_section (*mp, section, name, glob);

	  if (found && !findall)   /* i.e. only do this section... */
	    return found;
	}
    }
  else
    {
      for (sp = section_list; *sp != NULL; sp++)
	{
	  for (mp = manpathlist; *mp != NULL; mp++)
	    {
	      if (debug)
		fprintf (stderr, "\nsearching in %s\n", *mp);

	      glob = 1;

	      found += try_section (*mp, *sp, name, glob);

	      if (found && !findall)   /* i.e. only do this section... */
		return found;
	    }
	}
    }
  return found;
}

char **
get_section_list ()
{
  int i;
  char *p;
  char *end;
  static char *tmp_section_list[100];

  if (colon_sep_section_list == NULL)
    {
      if ((p = getenv ("MANSECT")) == NULL)
	{
	  return std_sections;
	}
      else
	{
	  colon_sep_section_list = strdup (p);
	}
    }

  i = 0;
  for (p = colon_sep_section_list; ; p = end+1)
    {
      if ((end = strchr (p, ':')) != NULL)
	*end = '\0';

      tmp_section_list[i++] = strdup (p);

      if (end == NULL)
	break;
    }

  tmp_section_list [i] = NULL;
  return tmp_section_list;
}
