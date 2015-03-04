/***************************************************************************
 * msi.c - Mini-SEED Inspector
 *
 * Opens a user specified file, parses the Mini-SEED records and prints
 * details for each record, trace list or gap list.
 *
 * In general critical error messages are prefixed with "ERROR:" and
 * the return code will be 1.  On successfull operation the return
 * code will be 0.
 *
 * Written by Chad Trabant, IRIS Data Management Center.
 *
 * modified 2015.062
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <regex.h>

#include <libmseed.h>

static int processparam (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static int readregexfile (char *regexfile, char **pppattern);
static int lisnumber (char *number);
static int addfile (char *filename);
static int addlistfile (char *filename);
static void usage (void);

#define VERSION "3.6"
#define PACKAGE "msi"

static flag    verbose      = 0;
static flag    ppackets     = 0;    /* Controls printing of header/blockettes */
static flag    printdata    = 0;    /* Controls printing of sample values: 1=first 6, 2=all*/
static flag    printraw     = 0;    /* Controls printing of header values in raw form */
static flag    printoffset  = 0;    /* Controls printing offset into input file */
static flag    printlatency = 0;    /* Controls printing latency based on system time */
static flag    basicsum     = 0;    /* Controls printing of basic summary */
static flag    tracegapsum  = 0;    /* Controls printing of trace or gap list */
static flag    tracegaponly = 0;    /* Controls printing of trace or gap list only */
static flag    tracegaps    = 0;    /* Controls printing of gaps with a trace list */
static flag    timeformat   = 0;    /* Time string format for trace or gap lists */
static flag    dataquality  = 0;    /* Control matching of data qualities */
static flag    skipnotdata  = 1;    /* Controls skipping of non-SEED data */
static double  timetol      = -1.0; /* Time tolerance for continuous traces */
static double  sampratetol  = -1.0; /* Sample rate tolerance for continuous traces */
static double  mingap       = 0;    /* Minimum gap/overlap seconds when printing gap list */
static double *mingapptr    = NULL;
static double  maxgap       = 0;    /* Maximum gap/overlap seconds when printing gap list */
static double *maxgapptr    = NULL;
static int     reccntdown   = -1;
static int     reclen       = -1;
static char   *encodingstr  = 0;
static char   *binfile      = 0;
static char   *outfile      = 0;
static hptime_t starttime   = HPTERROR;  /* Limit to records containing or after starttime */
static hptime_t endtime     = HPTERROR;  /* Limit to records containing or before endtime */
static regex_t *match       = 0;    /* Compiled match regex */
static regex_t *reject      = 0;    /* Compiled reject regex */

struct filelink {
  char *filename;
  uint64_t offset;
  struct filelink *next;
};

struct filelink *filelist = 0;
struct filelink *filelisttail = 0;


int
main (int argc, char **argv)
{
  struct filelink *flp;
  MSRecord *msr = 0;
  MSTraceList *mstl = 0;
  FILE *bfp = 0;
  FILE *ofp = 0;
  int retcode = MS_NOERROR;
  
  char envvariable[100];
  int dataflag = 0;
  long long int totalrecs  = 0;
  long long int totalsamps = 0;
  long long int totalfiles = 0;
  off_t filepos = 0;
  
  char srcname[50];
  char stime[30];
  
  /* Set default error message prefix */
  ms_loginit (NULL, NULL, NULL, "ERROR: ");
  
  /* Process given parameters (command line and parameter file) */
  if ( processparam (argc, argv) < 0 )
    return 1;
  
  /* Read leap second list file if env. var. LIBMSEED_LEAPSECOND_FILE is set */
  ms_readleapseconds ("LIBMSEED_LEAPSECOND_FILE");
  
  /* Setup encoding environment variable if specified, ugly kludge */
  if ( encodingstr )
    {
      snprintf (envvariable, sizeof(envvariable), "UNPACK_DATA_FORMAT=%s", encodingstr);
      
      if ( putenv (envvariable) )
	{
	  ms_log (2, "Cannot set environment variable UNPACK_DATA_FORMAT\n");
	  return 1;
	}
    }
  
  /* Open the integer output file if specified */
  if ( binfile )
    {
      if ( strcmp (binfile, "-") == 0 )
	{
	  bfp = stdout;
	}
      else if ( (bfp = fopen (binfile, "wb")) == NULL )
	{
	  ms_log (2, "Cannot open binary data output file: %s (%s)\n",
		  binfile, strerror(errno));
	  return 1;
	}
    }

  /* Open the output file if specified */
  if ( outfile )
    {
      if ( strcmp (outfile, "-") == 0 )
	{
	  ofp = stdout;
	}
      else if ( (ofp = fopen (outfile, "wb")) == NULL )
	{
	  ms_log (2, "Cannot open output file: %s (%s)\n",
		  outfile, strerror(errno));
	  return 1;
	}
    }
  
  if ( printdata || binfile )
    dataflag = 1;
  
  if ( tracegapsum || tracegaponly )
    mstl = mstl_init (NULL);
  
  flp = filelist;
  
  while ( flp != 0 )
    {
      if ( verbose >= 2 )
	{
	  if ( flp->offset )
	    ms_log (1, "Processing: %s (starting at byte %lld)\n", flp->filename, flp->offset);
	  else
	    ms_log (1, "Processing: %s\n", flp->filename);
	}
      
      /* Set starting byte offset if supplied as negative file position */
      filepos = - flp->offset;
      
      /* Loop over the input file */
      while ( reccntdown != 0 )
	{
	  if ( (retcode = ms_readmsr (&msr, flp->filename, reclen, &filepos,
				      NULL, skipnotdata, 0, verbose)) != MS_NOERROR )
	    break;
	  
	  /* Check if record matches start/end time criteria */
	  if ( starttime != HPTERROR || endtime != HPTERROR )
	    {
	      hptime_t recendtime = msr_endtime (msr);
	      
	      if ( starttime != HPTERROR && (msr->starttime < starttime && ! (msr->starttime <= starttime && recendtime >= starttime)) )
		{
		  if ( verbose >= 3 )
		    {
		      msr_srcname (msr, srcname, 1);
		      ms_hptime2seedtimestr (msr->starttime, stime, 1);
		      ms_log (1, "Skipping (starttime) %s, %s\n", srcname, stime);
		    }
		  continue;
		}
	      
	      if ( endtime != HPTERROR && (recendtime > endtime && ! (msr->starttime <= endtime && recendtime >= endtime)) )
		{
		  if ( verbose >= 3 )
		    {
		      msr_srcname (msr, srcname, 1);
		      ms_hptime2seedtimestr (msr->starttime, stime, 1);
		      ms_log (1, "Skipping (starttime) %s, %s\n", srcname, stime);
		    }
		  continue;
		}
	    }
	  
	  if ( match || reject )
	    {
	      /* Generate the srcname with the quality code */
	      msr_srcname (msr, srcname, 1);
	      
	      /* Check if record is matched by the match regex */
	      if ( match )
		{
		  if ( regexec ( match, srcname, 0, 0, 0) != 0 )
		    {
		      if ( verbose >= 3 )
			{
			  ms_hptime2seedtimestr (msr->starttime, stime, 1);
			  ms_log (1, "Skipping (match) %s, %s\n", srcname, stime);
			}
		      continue;
		    }
		}
	      
	      /* Check if record is rejected by the reject regex */
	      if ( reject )
		{
		  if ( regexec ( reject, srcname, 0, 0, 0) == 0 )
		    {
		      if ( verbose >= 3 )
			{
			  ms_hptime2seedtimestr (msr->starttime, stime, 1);
			  ms_log (1, "Skipping (reject) %s, %s\n", srcname, stime);
			}
		      continue;
		    }
		}
	    }
	  
	  if ( reccntdown > 0 )
	    reccntdown--;
	  
	  totalrecs++;
	  totalsamps += msr->samplecnt;
	  
	  if ( ! tracegaponly )
	    {
	      if ( printoffset )
		ms_log (0, "%-10lld", (long long) filepos);
	      
	      if ( printlatency )
		ms_log (0, "%-10.6g secs ", msr_host_latency(msr));
	      
	      if ( printraw )
		ms_parse_raw (msr->record, msr->reclen, ppackets, -1);
	      else
		msr_print (msr, ppackets);
	    }
	  
	  if ( tracegapsum || tracegaponly )
	    mstl_addmsr (mstl, msr, dataquality, 1, timetol, sampratetol);
	  
	  if ( dataflag )
	    {
	      /* Parse the record (again) and unpack the data */
	      int rv = msr_unpack (msr->record, msr->reclen, &msr, 1, verbose);
	      
	      if ( rv == MS_NOERROR && printdata && ! tracegaponly )
		{
		  int line, col, cnt, samplesize;
		  int lines = (msr->numsamples / 6) + 1;
		  void *sptr;
		  
		  if ( (samplesize = ms_samplesize(msr->sampletype)) == 0 )
		    {
		      ms_log (2, "Unrecognized sample type: %c\n", msr->sampletype);
		    }
		  
		  if ( msr->sampletype == 'a' )
		    {
		      char *ascii = (char *)msr->datasamples;
		      int length = msr->numsamples;
		      
		      ms_log (0, "ASCII Data:\n");
		      
		      /* Print maximum log message segments */
		      while ( length > (MAX_LOG_MSG_LENGTH-1) )
			{
			  ms_log (0, "%.*s", (MAX_LOG_MSG_LENGTH-1), ascii);
			  ascii += MAX_LOG_MSG_LENGTH-1;
			  length -= MAX_LOG_MSG_LENGTH-1;
			}
		      
		      /* Print any remaining ASCII and add a newline */
		      if ( length > 0 )
			{
			  ms_log (0, "%.*s\n", length, ascii);
			}
		      else
			{
			  ms_log (0, "\n");
			}
		    }
		  else
		    for ( cnt = 0, line = 0; line < lines; line++ )
		      {
			for ( col = 0; col < 6 ; col ++ )
			  {
			    if ( cnt < msr->numsamples )
			      {
				sptr = (char*)msr->datasamples + (cnt * samplesize);
				
				if ( msr->sampletype == 'i' )
				  ms_log (0, "%10d  ", *(int32_t *)sptr);
				
				else if ( msr->sampletype == 'f' )
				  ms_log (0, "%10.8g  ", *(float *)sptr);
				
				else if ( msr->sampletype == 'd' )
				  ms_log (0, "%10.10g  ", *(double *)sptr);
				
				cnt++;
			      }
			  }
			ms_log (0, "\n");
			
			/* If only printing the first 6 samples break out here */
			if ( printdata == 1 )
			  break;
		      }
		}
	      
	      if ( binfile )
		{
		  uint8_t samplesize = ms_samplesize (msr->sampletype);
		  
		  if ( samplesize )
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
	  
	  if ( outfile )
	    {
	      fwrite (msr->record, 1, msr->reclen, ofp);
	    }
	}
      
      /* Print error if not EOF and not counting down records */
      if ( retcode != MS_ENDOFFILE && reccntdown != 0 )
	{
	  ms_log (2, "Cannot read %s: %s\n", flp->filename, ms_errorstr(retcode));
	  ms_readmsr (&msr, NULL, 0, NULL, NULL, 0, 0, 0);
	  exit (1);
	}
      
      /* Make sure everything is cleaned up */
      ms_readmsr (&msr, NULL, 0, NULL, NULL, 0, 0, 0);
      
      totalfiles++;
      flp = flp->next;
    } /* End of looping over file list */
  
  if ( binfile )
    fclose (bfp);
  
  if ( outfile )
    fclose (ofp);
  
  if ( basicsum )
    ms_log (0, "Files: %lld, Records: %lld, Samples: %lld\n", totalfiles, totalrecs, totalsamps);
  
  if ( tracegapsum || tracegaponly )
    {
      if ( tracegapsum == 1 || tracegaponly == 1 )
	{
	  mstl_printtracelist (mstl, timeformat, 1, tracegaps);
	}
      if ( tracegapsum == 2 || tracegaponly == 2 )
	{
	  mstl_printgaplist (mstl, timeformat, mingapptr, maxgapptr);
	}
      if ( tracegaponly == 3 )
	{
	  mstl_printsynclist (mstl, NULL, 1);
	}
    }
  
  if ( mstl )
    mstl_free (&mstl, 0);
  
  return 0;
}  /* End of main() */


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
  char *matchpattern = 0;
  char *rejectpattern = 0;
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
	  usage();
	  exit (0);
	}
      else if (strncmp (argvec[optind], "-v", 2) == 0)
	{
	  verbose += strspn (&argvec[optind][1], "v");
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  reclen = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-e") == 0)
	{
	  encodingstr = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-ts") == 0)
	{
	  starttime = ms_seedtimestr2hptime (getoptval(argcount, argvec, optind++));
	  if ( starttime == HPTERROR )
	    return -1;
	}
      else if (strcmp (argvec[optind], "-te") == 0)
	{
	  endtime = ms_seedtimestr2hptime (getoptval(argcount, argvec, optind++));
	  if ( endtime == HPTERROR )
	    return -1;
	}
      else if (strcmp (argvec[optind], "-M") == 0)
	{
	  matchpattern = strdup (getoptval(argcount, argvec, optind++));
	}
      else if (strcmp (argvec[optind], "-R") == 0)
	{
	  rejectpattern = strdup (getoptval(argcount, argvec, optind++));
	}
      else if (strcmp (argvec[optind], "-n") == 0)
	{
	  reccntdown = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-y") == 0)
	{
	  skipnotdata = 0;
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
	  if ( ! tracegapsum )
	    tracegaponly = 1;
	}
      else if (strcmp (argvec[optind], "-tt") == 0)
	{
	  timetol = strtod (getoptval(argcount, argvec, optind++), NULL);
	}
      else if (strcmp (argvec[optind], "-rt") == 0)
	{
	  sampratetol = strtod (getoptval(argcount, argvec, optind++), NULL);
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
	  mingap = strtod (getoptval(argcount, argvec, optind++), NULL);
	  mingapptr = &mingap;
	}
      else if (strcmp (argvec[optind], "-gmax") == 0)
	{
	  maxgap = strtod (getoptval(argcount, argvec, optind++), NULL);
	  maxgapptr = &maxgap;
	}
      else if (strcmp (argvec[optind], "-Q") == 0)
	{
	  dataquality = 1;
	}
      else if (strcmp (argvec[optind], "-tf") == 0)
	{
	  timeformat = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-b") == 0)
	{
	  binfile = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-o") == 0)
	{
	  outfile = getoptval(argcount, argvec, optind++);
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
	       strlen (argvec[optind]) > 1 )
	{
	  ms_log (2, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
      else
	{
	  tptr = argvec[optind];
	  
          /* Check for an input file list */
          if ( tptr[0] == '@' )
            {
              if ( addlistfile (tptr+1) < 0 )
                {
                  ms_log (2, "Error adding list file %s", tptr+1);
                  exit (1);
                }
            }
          /* Otherwise this is an input file */
          else
            {
              /* Add file to global file list */
              if ( addfile (tptr) )
                {
                  ms_log (2, "Error adding file to input list %s", tptr);
                  exit (1);
                }
            }
	}
    }
  
  /* Make sure input file were specified */
  if ( filelist == 0 )
    {
      ms_log (2, "No input files were specified\n\n");
      ms_log (1, "%s version %s\n\n", PACKAGE, VERSION);
      ms_log (1, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }
  
  /* Expand match pattern from a file if prefixed by '@' */
  if ( matchpattern )
    {
      if ( *matchpattern == '@' )
	{
	  tptr = strdup(matchpattern + 1); /* Skip the @ sign */
	  free (matchpattern);
	  matchpattern = 0;
	  
	  if ( readregexfile (tptr, &matchpattern) <= 0 )
	    {
	      ms_log (2, "Cannot read match pattern regex file\n");
	      exit (1);
	    }
	  
	  free (tptr);
	}
    }
  
  /* Expand reject pattern from a file if prefixed by '@' */
  if ( rejectpattern )
    {
      if ( *rejectpattern == '@' )
	{
	  tptr = strdup(rejectpattern + 1); /* Skip the @ sign */
	  free (rejectpattern);
	  rejectpattern = 0;
	  
	  if ( readregexfile (tptr, &rejectpattern) <= 0 )
	    {
	      ms_log (2, "Cannot read reject pattern regex file\n");
	      exit (1);
	    }
	  
	  free (tptr);
	}
    }
  
  /* Compile match and reject patterns */
  if ( matchpattern )
    {
      match = (regex_t *) malloc (sizeof(regex_t));
      
      if ( regcomp (match, matchpattern, REG_EXTENDED) != 0)
	{
	  ms_log (2, "Cannot compile match regex: '%s'\n", matchpattern);
	}

      free (matchpattern);
    }
  
  if ( rejectpattern )
    {
      reject = (regex_t *) malloc (sizeof(regex_t));
      
      if ( regcomp (reject, rejectpattern, REG_EXTENDED) != 0)
	{
	  ms_log (2, "Cannot compile reject regex: '%s'\n", rejectpattern);
	}
      
      free (rejectpattern);
    }
  
  /* Report the program version */
  if ( verbose )
    ms_log (1, "%s version: %s\n", PACKAGE, VERSION);
  
  return 0;
}  /* End of parameter_proc() */


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
  if ( argvec == NULL || argvec[argopt] == NULL ) {
    ms_log (2, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }
  
  /* Special case of '-o -' usage */
  if ( (argopt+1) < argcount && strcmp (argvec[argopt], "-o") == 0 )
    if ( strcmp (argvec[argopt+1], "-") == 0 )
      return argvec[argopt+1];
  
  /* Special cases of '-gmin' and '-gmax' with negative numbers */
  if ( (argopt+1) < argcount &&
       (strcmp (argvec[argopt], "-gmin") == 0 || (strcmp (argvec[argopt], "-gmax") == 0)))
    if ( lisnumber(argvec[argopt+1]) )
      return argvec[argopt+1];
  
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  ms_log (2, "Option %s requires a value, try -h for usage\n", argvec[argopt]);
  exit (1);
  return 0;
}  /* End of getoptval() */


/***************************************************************************
 * readregexfile:
 *
 * Read a list of regular expressions from a file and combine them
 * into a single, compound expression which is returned in *pppattern.
 * The return buffer is reallocated as need to hold the growing
 * pattern.  When called *pppattern should not point to any associated
 * memory.
 *
 * Returns the number of regexes parsed from the file or -1 on error.
 ***************************************************************************/
static int
readregexfile (char *regexfile, char **pppattern)
{
  FILE *fp;
  char  line[1024];
  char  linepattern[1024];
  int   regexcnt = 0;
  int   lengthbase;
  int   lengthadd;
  
  if ( ! regexfile )
    {
      ms_log (2, "readregexfile: regex file not supplied\n");
      return -1;
    }
  
  if ( ! pppattern )
    {
      ms_log (2, "readregexfile: pattern string buffer not supplied\n");
      return -1;
    }
  
  /* Open the regex list file */
  if ( (fp = fopen (regexfile, "rb")) == NULL )
    {
      ms_log (2, "Cannot open regex list file %s: %s\n",
	      regexfile, strerror (errno));
      return -1;
    }
  
  if ( verbose )
    ms_log (1, "Reading regex list from %s\n", regexfile);
  
  *pppattern = NULL;
  
  while ( (fgets (line, sizeof(line), fp)) !=  NULL)
    {
      /* Trim spaces and skip if empty lines */
      if ( sscanf (line, " %s ", linepattern) != 1 )
	continue;
      
      /* Skip comment lines */
      if ( *linepattern == '#' )
	continue;
      
      regexcnt++;
      
      /* Add regex to compound regex */
      if ( *pppattern )
	{
	  lengthbase = strlen(*pppattern);
	  lengthadd = strlen(linepattern) + 4; /* Length of addition plus 4 characters: |()\0 */
	  
	  *pppattern = realloc (*pppattern, lengthbase + lengthadd);
	  
	  if ( *pppattern )
	    {
	      snprintf ((*pppattern)+lengthbase, lengthadd, "|(%s)", linepattern);
	    }
	  else
	    {
	      ms_log (2, "Cannot allocate memory for regex string\n");
	      return -1;
	    }
	}
      else
	{
	  lengthadd = strlen(linepattern) + 3; /* Length of addition plus 3 characters: ()\0 */
	  
	  *pppattern = malloc (lengthadd);
	  
	  if ( *pppattern )
	    {
	      snprintf (*pppattern, lengthadd, "(%s)", linepattern);
	    }
	  else
	    {
	      ms_log (2, "Cannot allocate memory for regex string\n");
	      return -1;
	    }
	}
    }
  
  fclose (fp);
  
  return regexcnt;
}  /* End of readregexfile() */


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
  
  while ( *(number+idx) )
    {
      if ( idx == 0 && *(number+idx) == '-' )
	{
	  idx++;
	  continue;
	}

      if ( ! isdigit ((int) *(number+idx)) )
	{
	  return 0;
	}

      idx++;
    }
  
  return 1;      
}  /* End of lisnumber() */


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
  char *at;
  
  if ( filename == NULL )
    {
      ms_log (2, "addfile(): No file name specified\n");
      return -1;
    }
  
  newlp = (struct filelink *) calloc (1, sizeof (struct filelink));
  
  if ( ! newlp )
    {
      ms_log (2, "addfile(): Cannot allocate memory\n");
      return -1;
    }
  
  /* Check for starting offset */
  if ( (at = strrchr (filename, '@')) )
    {
      *at++ = '\0';
      newlp->offset = strtoull (at, NULL, 10);
    }
  else
    {
      newlp->offset = 0;
    }
  
  newlp->filename = strdup(filename);
  
  if ( ! newlp->filename )
    {
      ms_log (2, "addfile(): Cannot duplicate string\n");
      return -1;
    }
  
  /* Add new file to the end of the list */
  if ( filelisttail == 0 )
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
}  /* End of addfile() */


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
  
  if ( verbose >= 1 )
    ms_log (1, "Reading list file '%s'\n", filename);
  
  if ( ! (fp = fopen(filename, "rb")) )
    {
      ms_log (2, "Cannot open list file %s: %s\n", filename, strerror(errno));
      return -1;
    }
  
  while ( fgets (filelistent, sizeof(filelistent), fp) )
    {
      char *cp;
      
      /* End string at first newline character */
      if ( (cp = strchr(filelistent, '\n')) )
        *cp = '\0';
      
      /* Skip empty lines */
      if ( ! strlen (filelistent) )
        continue;
      
      /* Skip comment lines */
      if ( *filelistent == '#' )
        continue;
      
      if ( verbose > 1 )
        ms_log (1, "Adding '%s' from list file\n", filelistent);
      
      if ( addfile (filelistent) )
        return -1;
      
      filecount++;
    }
  
  fclose (fp);
  
  return filecount;
}  /* End of addlistfile() */


/***************************************************************************
 * usage():
 * Print the usage message.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s - Mini-SEED Inspector version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Usage: %s [options] file1 [file2] [file3] ...\n\n", PACKAGE);
  fprintf (stderr,
	   " ## General options ##\n"
	   " -V           Report program version\n"
	   " -h           Show this usage message\n"
	   " -v           Be more verbose, multiple flags can be used\n"
	   " -r reclen    Specify record length in bytes, default is autodetection\n"
	   " -e encoding  Specify encoding format of data samples\n"
	   "\n"
	   " ## Data selection options ##\n"
	   " -ts time     Limit to records that start after time\n"
	   " -te time     Limit to records that end before time\n"
	   "                time format: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' delimiters: [,:.]\n"
	   " -M match     Limit to records matching the specified regular expression\n"
	   " -R reject    Limit to records not matching the specfied regular expression\n"
	   "                Regular expressions are applied to: 'NET_STA_LOC_CHAN_QUAL'\n"
	   " -n count     Only process count number of records\n"
	   " -y           Do not quietly skip non-SEED data\n"
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
	   " -Q           Additionally group traces by data quality\n"
	   " -tf format   Specify a time string format for trace and gap lists\n"
	   "                format: 0 = SEED time, 1 = ISO time, 2 = epoch time\n"
	   "\n"
	   " ## Data output options ##\n"
	   " -b binfile   Unpack/decompress data and write binary samples to binfile\n"
	   " -o outfile   Write processed records to outfile\n"
	   "\n"
	   " files        File(s) of Mini-SEED records, list files prefixed with '@'\n"
	   "\n");
}  /* End of usage() */
