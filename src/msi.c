/***************************************************************************
 * msi.c - miniSEED Inspector
 *
 * Opens a user specified file, parses the miniSEED records and prints
 * details for each record, trace list or gap list.
 *
 * In general critical error messages are prefixed with "ERROR:" and
 * the return code will be 1.  On successfull operation the return
 * code will be 0.
 *
 * Written by Chad Trabant, EarthScope Data Services
 ***************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libmseed.h>

static int processparam (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static int lisnumber (char *number);
static int addfile (char *filename);
static int addlistfile (char *filename);
static int my_globmatch (const char *string, const char *pattern);
static void usage (void);

#define VERSION "4.0"
#define PACKAGE "msi"

static int8_t verbose = 0;
static int8_t ppackets = 0; /* Controls printing of header/blockettes */
static int8_t printdata = 0; /* Controls printing of sample values: 1=first 6, 2=all*/
static int8_t printraw = 0; /* Controls printing of header values in raw form */
static int8_t printoffset = 0; /* Controls printing offset into input file */
static int8_t printlatency = 0; /* Controls printing latency based on system time */
static int8_t basicsum = 0; /* Controls printing of basic summary */
static int8_t tracegapsum = 0; /* Controls printing of trace or gap list */
static int8_t tracegaponly = 0; /* Controls printing of trace or gap list only */
static int8_t tracegaps = 0; /* Controls printing of gaps with a trace list */
static ms_timeformat_t timeformat = ISOMONTHDAY_Z; /* Time string format for trace or gap lists */
static int8_t splitversion = 0; /* Control grouping of data publication versions */
static int8_t skipnotdata = 0; /* Controls skipping of non-miniSEED data */
static double mingap = 0; /* Minimum gap/overlap seconds when printing gap list */
static double *mingapptr = NULL;
static double maxgap = 0; /* Maximum gap/overlap seconds when printing gap list */
static double *maxgapptr = NULL;
static int reccntdown = -1;
static char *binfile = NULL;
static char *outfile = NULL;
static nstime_t starttime = NSTERROR; /* Limit to records containing or after starttime */
static nstime_t endtime = NSTERROR; /* Limit to records containing or before endtime */
static char *match = NULL; /* Glob match pattern */
static char *reject = NULL; /* Glob reject pattern */

static double timetol; /* Time tolerance for continuous traces */
static double sampratetol; /* Sample rate tolerance for continuous traces */
static MS3Tolerance tolerance = { .time = NULL, .samprate = NULL };
double timetol_callback (const MS3Record *msr) { return timetol; }
double samprate_callback (const MS3Record *msr) { return sampratetol; }

struct filelink
{
  char *filename;
  struct filelink *next;
};

struct filelink *filelist = 0;
struct filelink *filelisttail = 0;

int
main (int argc, char **argv)
{
  struct filelink *flp;
  MS3Record *msr = 0;
  MS3TraceList *mstl = 0;
  MS3FileParam *msfp = NULL;
  FILE *bfp = 0;
  FILE *ofp = 0;
  int retcode = MS_NOERROR;

  uint32_t flags = 0;
  int dataflag = 0;
  int64_t totalrecs = 0;
  int64_t totalsamps = 0;
  int64_t totalfiles = 0;

  char stime[30];

  /* Set default error message prefix */
  ms_loginit (NULL, NULL, NULL, "ERROR: ");

  /* Process given parameters (command line and parameter file) */
  if (processparam (argc, argv) < 0)
    return 1;

  /* Read leap second list file if env. var. LIBMSEED_LEAPSECOND_FILE is set */
  ms_readleapseconds ("LIBMSEED_LEAPSECOND_FILE");

  /* Open the integer output file if specified */
  if (binfile)
  {
    if (strcmp (binfile, "-") == 0)
    {
      bfp = stdout;
    }
    else if ((bfp = fopen (binfile, "wb")) == NULL)
    {
      ms_log (2, "Cannot open binary data output file: %s (%s)\n",
              binfile, strerror (errno));
      return 1;
    }
  }

  /* Open the output file if specified */
  if (outfile)
  {
    if (strcmp (outfile, "-") == 0)
    {
      ofp = stdout;
    }
    else if ((ofp = fopen (outfile, "wb")) == NULL)
    {
      ms_log (2, "Cannot open output file: %s (%s)\n",
              outfile, strerror (errno));
      return 1;
    }
  }

  if (printdata || binfile)
    dataflag = 1;

  flags |= MSF_VALIDATECRC;
  flags |= MSF_PNAMERANGE;

  if (skipnotdata)
    flags |= MSF_SKIPNOTDATA;

  if (tracegapsum || tracegaponly)
    mstl = mstl3_init (NULL);

  flp = filelist;

  while (flp != 0)
  {
    if (verbose >= 2)
      ms_log (1, "Processing: %s\n", flp->filename);

    /* Loop over the input file */
    while (reccntdown != 0)
    {
      if ((retcode = ms3_readmsr_r (&msfp, &msr, flp->filename, flags, verbose)) != MS_NOERROR)
        break;

      /* Check if record matches start/end time criteria */
      if (starttime != NSTERROR || endtime != NSTERROR)
      {
        nstime_t recendtime = msr3_endtime (msr);

        if (starttime != NSTERROR && (msr->starttime < starttime && !(msr->starttime <= starttime && recendtime >= starttime)))
        {
          if (verbose >= 3)
          {
            ms_nstime2timestr (msr->starttime, stime, timeformat, NANO);
            ms_log (1, "Skipping (starttime) %s, %s\n", msr->sid, stime);
          }
          continue;
        }

        if (endtime != NSTERROR && (recendtime > endtime && !(msr->starttime <= endtime && recendtime >= endtime)))
        {
          if (verbose >= 3)
          {
            ms_nstime2timestr (msr->starttime, stime, timeformat, NANO);
            ms_log (1, "Skipping (starttime) %s, %s\n", msr->sid, stime);
          }
          continue;
        }
      }

      if (match || reject)
      {
        /* Check if record is matched by the match pattern */
        if (match)
        {
          if (my_globmatch (msr->sid, match) == 0)
          {
            if (verbose >= 3)
            {
              ms_nstime2timestr (msr->starttime, stime, timeformat, NANO);
              ms_log (1, "Skipping (match) %s, %s\n", msr->sid, stime);
            }
            continue;
          }
        }

        /* Check if record is rejected by the reject pattern */
        if (reject)
        {
          if (my_globmatch (msr->sid, reject) != 0)
          {
            if (verbose >= 3)
            {
              ms_nstime2timestr (msr->starttime, stime, timeformat, NANO);
              ms_log (1, "Skipping (reject) %s, %s\n", msr->sid, stime);
            }
            continue;
          }
        }
      }

      if (reccntdown > 0)
        reccntdown--;

      totalrecs++;
      totalsamps += msr->samplecnt;

      if (!tracegaponly)
      {
        if (printoffset)
          ms_log (0, "%-10" PRId64, (msfp->streampos - msr->reclen));

        if (printlatency)
          ms_log (0, "%-10.6g secs ", msr3_host_latency (msr));

        if (printraw)
        {
          if (msr->formatversion == 2)
            ms_parse_raw2 (msr->record, msr->reclen, ppackets, -1);
          else
            ms_parse_raw3 (msr->record, msr->reclen, ppackets);
        }
        else
          msr3_print (msr, ppackets);
      }

      if (tracegapsum || tracegaponly)
        mstl3_addmsr (mstl, msr, splitversion, flags, 1, &tolerance);

      if (dataflag)
      {
        /* Parse the record (again) and unpack the data */
        int unpacked = msr3_unpack_data (msr, verbose);

        if (unpacked > 0 && printdata && !tracegaponly)
        {
          int line, col, cnt, samplesize;
          int lines = (msr->numsamples / 6) + 1;
          void *sptr;

          if ((samplesize = ms_samplesize (msr->sampletype)) == 0)
          {
            ms_log (2, "Unrecognized sample type: %c\n", msr->sampletype);
          }

          if (msr->sampletype == 't')
          {
            char *textdata = (char *)msr->datasamples;
            int length = msr->numsamples;

            ms_log (0, "Text Data:\n");

            /* Print maximum log message segments */
            while (length > (MAX_LOG_MSG_LENGTH - 1))
            {
              ms_log (0, "%.*s", (MAX_LOG_MSG_LENGTH - 1), textdata);
              textdata += MAX_LOG_MSG_LENGTH - 1;
              length -= MAX_LOG_MSG_LENGTH - 1;
            }

            /* Print any remaining ASCII and add a newline */
            if (length > 0)
            {
              ms_log (0, "%.*s\n", length, textdata);
            }
            else
            {
              ms_log (0, "\n");
            }
          }
          else
            for (cnt = 0, line = 0; line < lines; line++)
            {
              for (col = 0; col < 6; col++)
              {
                if (cnt < msr->numsamples)
                {
                  sptr = (char *)msr->datasamples + (cnt * samplesize);

                  if (msr->sampletype == 'i')
                    ms_log (0, "%10d  ", *(int32_t *)sptr);

                  else if (msr->sampletype == 'f')
                    ms_log (0, "%10.8g  ", *(float *)sptr);

                  else if (msr->sampletype == 'd')
                    ms_log (0, "%10.10g  ", *(double *)sptr);

                  cnt++;
                }
              }
              ms_log (0, "\n");

              /* If only printing the first 6 samples break out here */
              if (printdata == 1)
                break;
            }
        }

        if (binfile)
        {
          uint8_t samplesize = ms_samplesize (msr->sampletype);

          if (samplesize)
          {
            fwrite (msr->datasamples, samplesize, msr->numsamples, bfp);
          }
          else
          {
            ms_log (1, "Cannot write to binary file, unknown sample type: %c\n",
                    msr->sampletype);
          }
        }
      }

      if (outfile)
      {
        fwrite (msr->record, 1, msr->reclen, ofp);
      }
    }

    /* Print error if not EOF and not counting down records */
    if (retcode != MS_ENDOFFILE && reccntdown != 0)
    {
      ms3_readmsr_r (&msfp, &msr, NULL, 0, 0);
      exit (1);
    }

    /* Make sure everything is cleaned up */
    ms3_readmsr_r (&msfp, &msr, NULL, 0, 0);

    totalfiles++;
    flp = flp->next;
  } /* End of looping over file list */

  if (binfile)
    fclose (bfp);

  if (outfile)
    fclose (ofp);

  if (basicsum)
    ms_log (0, "Files: %" PRId64 ", Records: %" PRId64 ", Samples: %" PRId64 "\n",
            totalfiles, totalrecs, totalsamps);

  if (tracegapsum || tracegaponly)
  {
    if (tracegapsum == 1 || tracegaponly == 1)
    {
      mstl3_printtracelist (mstl, timeformat, 1, tracegaps, 0);
    }
    if (tracegapsum == 2 || tracegaponly == 2)
    {
      mstl3_printgaplist (mstl, timeformat, mingapptr, maxgapptr);
    }
    if (tracegaponly == 3)
    {
      mstl3_printsynclist (mstl, NULL, 1);
    }
  }

  if (mstl)
    mstl3_free (&mstl, 0);

  return 0;
} /* End of main() */

/***************************************************************************
 * parameter_proc():
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
processparam (int argcount, char **argvec)
{
  int optind;
  int timeformat_option = -1;
  char *match_pattern = NULL;
  char *reject_pattern = NULL;
  char *tptr;

  /* Process all command line arguments */
  for (optind = 1; optind < argcount; optind++)
  {
    if (strcmp (argvec[optind], "-V") == 0)
    {
      ms_log (1, "%s version: %s\n", PACKAGE, VERSION);
      exit (0);
    }
    else if (strcmp (argvec[optind], "-h") == 0)
    {
      usage ();
      exit (0);
    }
    else if (strncmp (argvec[optind], "-v", 2) == 0)
    {
      verbose += strspn (&argvec[optind][1], "v");
    }
#if defined(LIBMSEED_URL)
    else if (strncmp (argvec[optind], "-H", 2) == 0)
    {
      if (ms3_url_addheader (getoptval (argcount, argvec, optind++)))
        return -1;
    }
    else if (strncmp (argvec[optind], "-u", 2) == 0)
    {
      if (ms3_url_userpassword (getoptval (argcount, argvec, optind++)))
        return -1;
    }
#endif /* LIBMSEED_URL */
    else if (strcmp (argvec[optind], "-ts") == 0)
    {
      starttime = ms_timestr2nstime (getoptval (argcount, argvec, optind++));
      if (starttime == NSTERROR)
        return -1;
    }
    else if (strcmp (argvec[optind], "-te") == 0)
    {
      endtime = ms_timestr2nstime (getoptval (argcount, argvec, optind++));
      if (endtime == NSTERROR)
        return -1;
    }
    else if (strcmp (argvec[optind], "-m") == 0)
    {
      match_pattern = strdup (getoptval (argcount, argvec, optind++));
    }
    else if (strcmp (argvec[optind], "-r") == 0)
    {
      reject_pattern = strdup (getoptval (argcount, argvec, optind++));
    }
    else if (strcmp (argvec[optind], "-n") == 0)
    {
      reccntdown = strtol (getoptval (argcount, argvec, optind++), NULL, 10);
    }
    else if (strcmp (argvec[optind], "-snd") == 0)
    {
      skipnotdata = 1;
    }
    else if (strncmp (argvec[optind], "-p", 2) == 0)
    {
      ppackets += strspn (&argvec[optind][1], "p");
    }
    else if (strcmp (argvec[optind], "-O") == 0)
    {
      printoffset = 1;
    }
    else if (strcmp (argvec[optind], "-L") == 0)
    {
      printlatency = 1;
    }
    else if (strcmp (argvec[optind], "-s") == 0)
    {
      basicsum = 1;
    }
    else if (strcmp (argvec[optind], "-d") == 0)
    {
      printdata = 1;
    }
    else if (strcmp (argvec[optind], "-D") == 0)
    {
      printdata = 2;
    }
    else if (strcmp (argvec[optind], "-z") == 0)
    {
      printraw = 1;
    }
    else if (strcmp (argvec[optind], "-t") == 0)
    {
      tracegapsum = 1;
    }
    else if (strcmp (argvec[optind], "-T") == 0)
    {
      tracegaponly = 1;
    }
    else if (strcmp (argvec[optind], "-tg") == 0)
    {
      tracegaps = 1;

      /* -T is assumed if -t/-g is not already set */
      if (!tracegapsum)
        tracegaponly = 1;
    }
    else if (strcmp (argvec[optind], "-tt") == 0)
    {
      timetol = strtod (getoptval (argcount, argvec, optind++), NULL);
      tolerance.time = timetol_callback;
    }
    else if (strcmp (argvec[optind], "-rt") == 0)
    {
      sampratetol = strtod (getoptval (argcount, argvec, optind++), NULL);
      tolerance.samprate = samprate_callback;
    }
    else if (strcmp (argvec[optind], "-g") == 0)
    {
      tracegapsum = 2;
    }
    else if (strcmp (argvec[optind], "-G") == 0)
    {
      tracegaponly = 2;
    }
    else if (strcmp (argvec[optind], "-S") == 0)
    {
      tracegaponly = 3;
    }
    else if (strcmp (argvec[optind], "-gmin") == 0)
    {
      mingap = strtod (getoptval (argcount, argvec, optind++), NULL);
      mingapptr = &mingap;
    }
    else if (strcmp (argvec[optind], "-gmax") == 0)
    {
      maxgap = strtod (getoptval (argcount, argvec, optind++), NULL);
      maxgapptr = &maxgap;
    }
    else if (strcmp (argvec[optind], "-Q") == 0 ||
             strcmp (argvec[optind], "-P") == 0)
    {
      splitversion = 1;
    }
    else if (strcmp (argvec[optind], "-tf") == 0)
    {
      timeformat_option = strtol (getoptval (argcount, argvec, optind++), NULL, 10);
    }
    else if (strcmp (argvec[optind], "-b") == 0)
    {
      binfile = getoptval (argcount, argvec, optind++);
    }
    else if (strcmp (argvec[optind], "-o") == 0)
    {
      outfile = getoptval (argcount, argvec, optind++);
    }
    else if (strncmp (argvec[optind], "-", 1) == 0 &&
             strlen (argvec[optind]) > 1)
    {
      ms_log (2, "Unknown option: %s\n", argvec[optind]);
      exit (1);
    }
    else
    {
      tptr = argvec[optind];

      /* Check for an input file list */
      if (tptr[0] == '@')
      {
        if (addlistfile (tptr + 1) < 0)
        {
          ms_log (2, "Error adding list file %s", tptr + 1);
          exit (1);
        }
      }
      /* Otherwise this is an input file */
      else
      {
        /* Add file to global file list */
        if (addfile (tptr))
        {
          ms_log (2, "Error adding file to input list %s", tptr);
          exit (1);
        }
      }
    }
  }

  if (timeformat_option >= 0)
  {
    switch (timeformat_option)
    {
    case 0:
      timeformat = SEEDORDINAL;
      break;
    case 1:
      timeformat = ISOMONTHDAY;
      break;
    case 2:
      timeformat = UNIXEPOCH;
      break;
    default:
      ms_log (2, "Invalid time format (-tf) value: %d\n\n", timeformat_option);
      exit(1);
    }
  }

  /* Make sure input file were specified */
  if (!filelist)
  {
    ms_log (2, "No input files were specified\n\n");
    ms_log (1, "%s version %s\n\n", PACKAGE, VERSION);
    ms_log (1, "Try %s -h for usage\n", PACKAGE);
    exit (1);
  }

  /* Add wildcards to match pattern for logical "contains" */
  if (match_pattern)
  {
    if ((match = malloc (strlen (match_pattern) + 3)) == NULL)
    {
      ms_log (2, "Error allocating memory\n");
      exit (1);
    }

    snprintf (match, strlen (match_pattern) + 3, "*%s*", match_pattern);
  }

  /* Add wildcards to reject pattern for logical "contains" */
  if (reject_pattern)
  {
    if ((reject = malloc (strlen (reject_pattern) + 3)) == NULL)
    {
      ms_log (2, "Error allocating memory\n");
      exit (1);
    }

    snprintf (reject, strlen (reject_pattern) + 3, "*%s*", reject_pattern);
  }

  /* Add program name and version to User-Agent for URL-based requests */
  if (libmseed_url_support() && ms3_url_useragent(PACKAGE, VERSION))
    return -1;

  /* Report the program version */
  if (verbose)
    ms_log (1, "%s version: %s\n", PACKAGE, VERSION);

  return 0;
} /* End of parameter_proc() */

/***************************************************************************
 * getoptval:
 * Return the value to a command line option; checking that the value is
 * itself not an option (starting with '-') and is not past the end of
 * the argument list.
 *
 * argcount: total arguments in argvec
 * argvec: argument list
 * argopt: index of option to process, value is expected to be at argopt+1
 *
 * Returns value on success and exits with error message on failure
 ***************************************************************************/
static char *
getoptval (int argcount, char **argvec, int argopt)
{
  if (argvec == NULL || argvec[argopt] == NULL)
  {
    ms_log (2, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }

  /* Special case of '-o -' usage */
  if ((argopt + 1) < argcount && strcmp (argvec[argopt], "-o") == 0)
    if (strcmp (argvec[argopt + 1], "-") == 0)
      return argvec[argopt + 1];

  /* Special cases of '-gmin' and '-gmax' with negative numbers */
  if ((argopt + 1) < argcount &&
      (strcmp (argvec[argopt], "-gmin") == 0 || (strcmp (argvec[argopt], "-gmax") == 0)))
    if (lisnumber (argvec[argopt + 1]))
      return argvec[argopt + 1];

  if ((argopt + 1) < argcount && *argvec[argopt + 1] != '-')
    return argvec[argopt + 1];

  ms_log (2, "Option %s requires a value, try -h for usage\n", argvec[argopt]);
  exit (1);
  return 0;
} /* End of getoptval() */

/***************************************************************************
 * lisnumber:
 *
 * Test if the string is all digits allowing an initial minus sign.
 *
 * Return 0 if not a number otherwise 1.
 ***************************************************************************/
static int
lisnumber (char *number)
{
  int idx = 0;

  while (*(number + idx))
  {
    if (idx == 0 && *(number + idx) == '-')
    {
      idx++;
      continue;
    }

    if (!isdigit ((int)*(number + idx)))
    {
      return 0;
    }

    idx++;
  }

  return 1;
} /* End of lisnumber() */

/***************************************************************************
 * addfile:
 *
 * Add file to end of the global file list (filelist).
 *
 * Returns 0 on success and -1 on error.
 ***************************************************************************/
static int
addfile (char *filename)
{
  struct filelink *newlp;

  if (filename == NULL)
  {
    ms_log (2, "addfile(): No file name specified\n");
    return -1;
  }

  newlp = (struct filelink *)calloc (1, sizeof (struct filelink));

  if (!newlp)
  {
    ms_log (2, "addfile(): Cannot allocate memory\n");
    return -1;
  }

  newlp->filename = strdup (filename);

  if (!newlp->filename)
  {
    ms_log (2, "addfile(): Cannot duplicate string\n");
    return -1;
  }

  /* Add new file to the end of the list */
  if (!filelisttail)
  {
    filelist = newlp;
    filelisttail = newlp;
  }
  else
  {
    filelisttail->next = newlp;
    filelisttail = newlp;
  }

  return 0;
} /* End of addfile() */

/***************************************************************************
 * addlistfile:
 *
 * Add files listed in the specified file to the global input file list.
 *
 * Returns count of files added on success and -1 on error.
 ***************************************************************************/
static int
addlistfile (char *filename)
{
  FILE *fp;
  char filelistent[1024];
  int filecount = 0;

  if (verbose >= 1)
    ms_log (1, "Reading list file '%s'\n", filename);

  if (!(fp = fopen (filename, "rb")))
  {
    ms_log (2, "Cannot open list file %s: %s\n", filename, strerror (errno));
    return -1;
  }

  while (fgets (filelistent, sizeof (filelistent), fp))
  {
    char *cp;

    /* End string at first newline character */
    if ((cp = strchr (filelistent, '\n')))
      *cp = '\0';

    /* Skip empty lines */
    if (!strlen (filelistent))
      continue;

    /* Skip comment lines */
    if (*filelistent == '#')
      continue;

    if (verbose > 1)
      ms_log (1, "Adding '%s' from list file\n", filelistent);

    if (addfile (filelistent))
      return -1;

    filecount++;
  }

  fclose (fp);

  return filecount;
} /* End of addlistfile() */

/***********************************************************************
 * robust glob pattern matcher
 * ozan s. yigit/dec 1994
 * public domain
 *
 * glob patterns:
 *	*	matches zero or more characters
 *	?	matches any single character
 *	[set]	matches any character in the set
 *	[^set]	matches any character NOT in the set
 *		where a set is a group of characters or ranges. a range
 *		is written as two characters seperated with a hyphen: a-z denotes
 *		all characters between a to z inclusive.
 *	[-set]	set matches a literal hypen and any character in the set
 *	[]set]	matches a literal close bracket and any character in the set
 *
 *	char	matches itself except where char is '*' or '?' or '['
 *	\char	matches char, including any pattern character
 *
 * examples:
 *	a*c		ac abc abbc ...
 *	a?c		acc abc aXc ...
 *	a[a-z]c		aac abc acc ...
 *	a[-a-z]c	a-c aac abc ...
 *
 * Revision 1.4  2004/12/26  12:38:00  ct
 * Changed function name (amatch -> globmatch), variables and
 * formatting for clarity.  Also add matching header globmatch.h.
 *
 * Revision 1.3  1995/09/14  23:24:23  oz
 * removed boring test/main code.
 *
 * Revision 1.2  94/12/11  10:38:15  oz
 * charset code fixed. it is now robust and interprets all
 * variations of charset [i think] correctly, including [z-a] etc.
 *
 * Revision 1.1  94/12/08  12:45:23  oz
 * Initial revision
 ***********************************************************************/

#define GLOBMATCH_TRUE 1
#define GLOBMATCH_FALSE 0
#define GLOBMATCH_NEGATE '^' /* std char set negation char */

/***********************************************************************
 * my_globmatch:
 *
 * Check if a string matches a globbing pattern.
 *
 * Return 0 if string does not match pattern and non-zero otherwise.
 **********************************************************************/
static int
my_globmatch (const char *string, const char *pattern)
{
  int negate;
  int match;
  int c;

  while (*pattern)
  {
    if (!*string && *pattern != '*')
      return GLOBMATCH_FALSE;

    switch (c = *pattern++)
    {

    case '*':
      while (*pattern == '*')
        pattern++;

      if (!*pattern)
        return GLOBMATCH_TRUE;

      if (*pattern != '?' && *pattern != '[' && *pattern != '\\')
        while (*string && *pattern != *string)
          string++;

      while (*string)
      {
        if (my_globmatch (string, pattern))
          return GLOBMATCH_TRUE;
        string++;
      }
      return GLOBMATCH_FALSE;

    case '?':
      if (*string)
        break;
      return GLOBMATCH_FALSE;

      /* set specification is inclusive, that is [a-z] is a, z and
       * everything in between. this means [z-a] may be interpreted
       * as a set that contains z, a and nothing in between.
       */
    case '[':
      if (*pattern != GLOBMATCH_NEGATE)
        negate = GLOBMATCH_FALSE;
      else
      {
        negate = GLOBMATCH_TRUE;
        pattern++;
      }

      match = GLOBMATCH_FALSE;

      while (!match && (c = *pattern++))
      {
        if (!*pattern)
          return GLOBMATCH_FALSE;

        if (*pattern == '-') /* c-c */
        {
          if (!*++pattern)
            return GLOBMATCH_FALSE;
          if (*pattern != ']')
          {
            if (*string == c || *string == *pattern ||
                (*string > c && *string < *pattern))
              match = GLOBMATCH_TRUE;
          }
          else
          { /* c-] */
            if (*string >= c)
              match = GLOBMATCH_TRUE;
            break;
          }
        }
        else /* cc or c] */
        {
          if (c == *string)
            match = GLOBMATCH_TRUE;
          if (*pattern != ']')
          {
            if (*pattern == *string)
              match = GLOBMATCH_TRUE;
          }
          else
            break;
        }
      }

      if (negate == match)
        return GLOBMATCH_FALSE;

      /* If there is a match, skip past the charset and continue on */
      while (*pattern && *pattern != ']')
        pattern++;
      if (!*pattern++) /* oops! */
        return GLOBMATCH_FALSE;
      break;

    case '\\':
      if (*pattern)
        c = *pattern++;
    default:
      if (c != *string)
        return GLOBMATCH_FALSE;
      break;
    }

    string++;
  }

  return !*string;
} /* End of my_globmatch() */

/***************************************************************************
 * usage():
 * Print the usage message.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s - miniSEED Inspector version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Usage: %s [options] file1 [file2] [file3] ...\n\n", PACKAGE);
  fprintf (stderr,
           " ## General options ##\n"
           " -V           Report program version\n"
           " -h           Show this usage message\n"
           " -v           Be more verbose, multiple flags can be used\n"
#if defined(LIBMSEED_URL)
           " -H header    Add custom header to URL-based reads\n"
           " -u user:pass Set username:password credentials for URL-based reads\n"
#endif /* LIBMSEED_URL */
           "\n"
           " ## Data selection options ##\n"
           " -ts time     Limit to records that start after time\n"
           " -te time     Limit to records that end before time\n"
           "                time format: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' delimiters: [,:.]\n"
           " -m match     Limit to records containing the specified pattern\n"
           " -r reject    Limit to records not containing the specfied pattern\n"
           "                Patterns are applied to: 'FDSN:NET_STA_LOC_BAND_SOURCE_SS'\n"
           " -n count     Only process count number of records\n"
           " -snd         Skip non-miniSEED data\n"
           "\n"
           " ## Output options ##\n"
           " -p           Print details of header, multiple flags can be used\n"
           " -O           Include offset into file when printing header details\n"
           " -L           Include latency when printing header details\n"
           " -s           Print a basic summary after processing file(s)\n"
           " -d           Unpack/decompress data and print the first 6 samples/record\n"
           " -D           Unpack/decompress data and print all samples\n"
           " -z           Validate and print record details in a raw form\n"
           "\n"
           " ## Trace and gap list output options ##\n"
           " -t           Print a sorted trace list after processing file(s)\n"
           " -T           Only print a sorted trace list\n"
           " -tg          Include gap estimates when printing trace list\n"
           " -tt secs     Specify a time tolerance for continuous traces\n"
           " -rt diff     Specify a sample rate tolerance for continuous traces\n"
           " -g           Print a sorted gap/overlap list after processing file(s)\n"
           " -G           Only print a sorted gap/overlap list\n"
           " -gmin secs   Only report gaps/overlaps larger or equal to specified seconds\n"
           " -gmax secs   Only report gaps/overlaps smaller or equal to specified seconds\n"
           " -S           Print a SYNC trace summary\n"
           " -P           Additionally group traces by data publication version\n"
           " -tf format   Specify a time string format for trace and gap lists\n"
           "                format: 0 = SEED time, 1 = ISO time, 2 = epoch time\n"
           "\n"
           " ## Data output options ##\n"
           " -b binfile   Unpack/decompress data and write binary samples to binfile\n"
           " -o outfile   Write processed records to outfile\n"
           "\n"
           " files        File(s) of miniSEED records, list files prefixed with '@'\n"
           "\n");
} /* End of usage() */
