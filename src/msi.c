/***************************************************************************
 * msi.c - Mini-SEED Inspector
 *
 * A rather useful example of using the libmseed Mini-SEED library.
 *
 * Opens a user specified file, parses the Mini-SEED records and prints
 * details for each record, trace list or gap list.
 *
 * Written by Chad Trabant, IRIS Data Management Center.
 *
 * modified 2006.279
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
static void addfile (char *filename);
static void usage (void);

#define VERSION "2.0pre"
#define PACKAGE "msi"

static flag    verbose      = 0;
static flag    ppackets     = 0;    /* Controls printing of header/blockettes */
static flag    printdata    = 0;    /* Controls printing of sample values: 1=first 6, 2=all*/
static flag    printoffset  = 0;    /* Controls printing offset into input file */
static flag    basicsum     = 0;    /* Controls printing of basic summary */
static flag    tracegapsum  = 0;    /* Controls printing of trace or gap list */
static flag    tracegaponly = 0;    /* Controls printing of trace or gap list only */
static flag    tracegaps    = 0;    /* Controls printing of gaps with a trace list */
static flag    timeformat   = 0;    /* Time string format for trace or gap lists */
static flag    dataquality  = 0;    /* Control matching of data qualities */
static double  timetol      = -1.0; /* Time tolerance for continuous traces */
static double  sampratetol  = -1.0; /* Sample rate tolerance for continuous traces */
static double  mingap       = 0;    /* Minimum gap/overlap seconds when printing gap list */
static double *mingapptr    = NULL;
static double  maxgap       = 0;    /* Maximum gap/overlap seconds when printing gap list */
static double *maxgapptr    = NULL;
static flag    traceheal    = 0;    /* Controls healing of trace group */
static int     reccntdown   = -1;
static int     reclen       = -1;
static char   *encodingstr  = 0;
static char   *binfile      = 0;
static char   *outfile      = 0;
static hptime_t starttime   = HPTERROR;  /* Limit to records after starttime */
static hptime_t endtime     = HPTERROR;  /* Limit to records before endtime */
static regex_t *match       = 0;    /* Compiled match regex */
static regex_t *reject      = 0;    /* Compiled reject regex */

struct filelink {
  char *filename;
  struct filelink *next;
};

struct filelink *filelist = 0;


int
main (int argc, char **argv)
{
  struct filelink *flp;
  MSRecord *msr = 0;
  MSTraceGroup *mstg = 0;
  FILE *bfp = 0;
  FILE *ofp = 0;
  int retcode = MS_NOERROR;

  char envvariable[100];
  int dataflag   = 0;
  long long int totalrecs  = 0;
  long long int totalsamps = 0;
  long long int totalfiles = 0;
  off_t filepos  = 0;
  
  char basesrc[50];
  char srcname[50];
  char stime[30];
  
  /* Process given parameters (command line and parameter file) */
  if ( processparam (argc, argv) < 0 )
    return -1;
  
  /* Setup encoding environment variable if specified, ugly kludge */
  if ( encodingstr )
    {
      snprintf (envvariable, sizeof(envvariable), "UNPACK_DATA_FORMAT=%s", encodingstr);
      
      if ( putenv (envvariable) )
	{
	  fprintf (stderr, "Error setting environment variable UNPACK_DATA_FORMAT\n");
	  return -1;
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
	  fprintf (stderr, "Cannot open binary data output file: %s (%s)\n",
		   binfile, strerror(errno));
	  return -1;
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
	  fprintf (stderr, "Cannot open output file: %s (%s)\n",
		   outfile, strerror(errno));
	  return -1;
	}
    }
  
  if ( printdata || binfile )
    dataflag = 1;
  
  if ( tracegapsum || tracegaponly )
    mstg = mst_initgroup (NULL);
  
  flp = filelist;

  while ( flp != 0 )
    {
      if ( verbose >= 2 )
	fprintf (stderr, "Processing: %s\n", flp->filename);
      
      /* Loop over the input file */
      while ( reccntdown != 0 )
	{
	  if ( (retcode = ms_readmsr (&msr, flp->filename, reclen, &filepos,
				      NULL, 1, dataflag, verbose)) != MS_NOERROR )
	    break;
	  
	  /* Check if record matches start/end time criteria */
	  if ( starttime != HPTERROR && (msr->starttime < starttime) )
	    {
	      if ( verbose >= 3 )
		{
		  msr_srcname (msr, srcname);
		  ms_hptime2seedtimestr (msr->starttime, stime);
		  fprintf (stderr, "Skipping (starttime) %s, %s\n", srcname, stime);
		}
	      continue;
	    }
	  
	  if ( endtime != HPTERROR && (msr_endtime(msr) > endtime) )
	    {
	      if ( verbose >= 3 )
		{
		  msr_srcname (msr, srcname);
		  ms_hptime2seedtimestr (msr->starttime, stime);
		  fprintf (stderr, "Skipping (starttime) %s, %s\n", srcname, stime);
		}
	      continue;
	    }
	  
	  if ( match || reject )
	    {
	      /* Generate the srcname and add the quality code */
	      msr_srcname (msr, basesrc);
	      snprintf (srcname, sizeof(srcname), "%s_%c", basesrc, msr->dataquality);
	      
	      /* Check if record is matched by the match regex */
	      if ( match )
		{
		  if ( regexec ( match, srcname, 0, 0, 0) != 0 )
		    {
		      if ( verbose >= 3 )
			{
			  ms_hptime2seedtimestr (msr->starttime, stime);
			  fprintf (stderr, "Skipping (match) %s, %s\n", srcname, stime);
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
			  ms_hptime2seedtimestr (msr->starttime, stime);
			  fprintf (stderr, "Skipping (reject) %s, %s\n", srcname, stime);
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
		printf ("%-10lld", (long long) filepos);
	      
	      msr_print (msr, ppackets);
	    }
	  
	  if ( tracegapsum || tracegaponly )
	    mst_addmsrtogroup (mstg, msr, dataquality, timetol, sampratetol);
	  
	  if ( dataflag )
	    {
	      if ( printdata && ! tracegaponly )
		{
		  int line, col, cnt, samplesize;
		  int lines = (msr->numsamples / 6) + 1;
		  void *sptr;
		  
		  if ( (samplesize = get_samplesize(msr->sampletype)) == 0 )
		    {
		      fprintf (stderr, "Unrecognized sample type: %c\n", msr->sampletype);
		    }
		  
		  if ( msr->sampletype == 'a' )
		    printf ("ASCII Data:\n%.*s\n", msr->numsamples, (char *)msr->datasamples);
		  else
		    for ( cnt = 0, line = 0; line < lines; line++ )
		      {
			for ( col = 0; col < 6 ; col ++ )
			  {
			    if ( cnt < msr->numsamples )
			      {
				sptr = (char*)msr->datasamples + (cnt * samplesize);
				
				if ( msr->sampletype == 'i' )
				  printf ("%10d  ", *(int32_t *)sptr);
				
				else if ( msr->sampletype == 'f' )
				  printf ("%10.8g  ", *(float *)sptr);
				
				else if ( msr->sampletype == 'd' )
				  printf ("%10.10g  ", *(double *)sptr);
				
				cnt++;
			      }
			  }
			printf ("\n");
			
			/* If only printing the first 6 samples break out here */
			if ( printdata == 1 )
			  break;
		      }
		}
	      if ( binfile )
		{
		  fwrite (msr->datasamples, 4, msr->numsamples, bfp);
		}
	    }

	  if ( outfile )
	    {
	      fwrite (msr->record, 1, msr->reclen, ofp);
	    }
	}
      
      /* Print error if not EOF and not counting down records */
      if ( retcode != MS_ENDOFFILE && reccntdown != 0 )
	fprintf (stderr, "Error reading %s: %s\n",
		 flp->filename, get_errorstr(retcode));
      
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
    printf ("Files: %lld, Records: %lld, Samples: %lld\n", totalfiles, totalrecs, totalsamps);
    
  if ( tracegapsum || tracegaponly )
    {
      mst_groupsort (mstg);
      
      if ( traceheal )
	mst_groupheal (mstg, -1.0, -1.0);
      
      if ( tracegapsum == 1 || tracegaponly == 1 )
	{
	  mst_printtracelist (mstg, timeformat, 1, tracegaps);
	}
      if ( tracegapsum == 2 || tracegaponly == 2 )
	{
	  mst_printgaplist (mstg, timeformat, mingapptr, maxgapptr);
	}
    }
  
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
	  fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
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
	  matchpattern = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-R") == 0)
	{
	  rejectpattern = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-n") == 0)
	{
	  reccntdown = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strncmp (argvec[optind], "-p", 2) == 0)
	{
	  ppackets += strspn (&argvec[optind][1], "p");
	}
      else if (strcmp (argvec[optind], "-O") == 0)
	{
	  printoffset = 1;
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
      else if (strcmp (argvec[optind], "-min") == 0)
	{
	  mingap = strtod (getoptval(argcount, argvec, optind++), NULL);
	  mingapptr = &mingap;
	}
      else if (strcmp (argvec[optind], "-max") == 0)
	{
	  maxgap = strtod (getoptval(argcount, argvec, optind++), NULL);
	  maxgapptr = &maxgap;
	}
      else if (strcmp (argvec[optind], "-Q") == 0)
	{
	  dataquality = 1;
	}
      else if (strcmp (argvec[optind], "-H") == 0)
	{
	  traceheal = 1;
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
	  fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
      else
	{
	  addfile (argvec[optind]);
	}
    }

  /* Make sure input file were specified */
  if ( filelist == 0 )
    {
      fprintf (stderr, "No input files were specified\n\n");
      fprintf (stderr, "%s version %s\n\n", PACKAGE, VERSION);
      fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }
  
  /* Expand match pattern from a file if prefixed by '@' */
  if ( matchpattern )
    {
      if ( *matchpattern == '@' )
	{
	  tptr = matchpattern + 1; /* Skip the @ sign */
	  matchpattern = 0;
	  
	  if ( readregexfile (tptr, &matchpattern) <= 0 )
	    {
	      fprintf (stderr, "ERROR reading match pattern regex file\n");
	      exit (1);
	    }
	}
    }
  
  /* Expand reject pattern from a file if prefixed by '@' */
  if ( rejectpattern )
    {
      if ( *rejectpattern == '@' )
	{
	  tptr = rejectpattern + 1; /* Skip the @ sign */
	  rejectpattern = 0;
	  
	  if ( readregexfile (tptr, &rejectpattern) <= 0 )
	    {
	      fprintf (stderr, "ERROR reading reject pattern regex file\n");
	      exit (1);
	    }
	}
    }
  
  /* Compile match and reject patterns */
  if ( matchpattern )
    {
      match = (regex_t *) malloc (sizeof(regex_t));
      
      if ( regcomp (match, matchpattern, REG_EXTENDED) != 0)
	{
	  fprintf (stderr, "ERROR compiling match regex: '%s'\n", matchpattern);
	}
    }
  
  if ( rejectpattern )
    {
      reject = (regex_t *) malloc (sizeof(regex_t));
      
      if ( regcomp (reject, rejectpattern, REG_EXTENDED) != 0)
	{
	  fprintf (stderr, "ERROR compiling reject regex: '%s'\n", rejectpattern);
	}
    }
  
  /* Report the program version */
  if ( verbose )
    fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);

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
    fprintf (stderr, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }
  
  /* Special case of '-o -' usage */
  if ( (argopt+1) < argcount && strcmp (argvec[argopt], "-o") == 0 )
    if ( strcmp (argvec[argopt+1], "-") == 0 )
      return argvec[argopt+1];
  
  /* Special cases of '-min' and '-max' with negative numbers */
  if ( (argopt+1) < argcount &&
       (strcmp (argvec[argopt], "-min") == 0 || (strcmp (argvec[argopt], "-max") == 0)))
    if ( lisnumber(argvec[argopt+1]) )
      return argvec[argopt+1];
  
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  fprintf (stderr, "Option %s requires a value\n", argvec[argopt]);
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
  int   newpatternsize;
  
  /* Open the regex list file */
  if ( (fp = fopen (regexfile, "rb")) == NULL )
    {
      fprintf (stderr, "ERROR opening regex list file %s: %s\n",
	       regexfile, strerror (errno));
      return -1;
    }
  
  if ( verbose )
    fprintf (stderr, "Reading regex list from %s\n", regexfile);
  
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
	  newpatternsize = strlen(*pppattern) + strlen(linepattern) + 4;
	  *pppattern = realloc (*pppattern, newpatternsize);	  
	  snprintf (*pppattern, newpatternsize, "%s|(%s)", *pppattern, linepattern);
	}
      else
	{
	  newpatternsize = strlen(linepattern) + 3;
	  *pppattern = realloc (*pppattern, newpatternsize);
	  snprintf (*pppattern, newpatternsize, "(%s)", linepattern);
	}
    }
  
  fclose (fp);
  
  return regexcnt;
}  /* End readregexfile() */


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
 ***************************************************************************/
static void
addfile (char *filename)
{
  struct filelink *lastlp, *newlp;
  
  if ( filename == NULL )
    {
      fprintf (stderr, "addfile(): No file name specified\n");
      return;
    }
  
  lastlp = filelist;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
	break;
      
      lastlp = lastlp->next;
    }
  
  newlp = (struct filelink *) malloc (sizeof (struct filelink));
  newlp->filename = strdup(filename);
  newlp->next = 0;
  
  if ( lastlp == 0 )
    filelist = newlp;
  else
    lastlp->next = newlp;
  
}  /* End of addfile() */


/***************************************************************************
 * usage():
 * Print the usage message and exit.
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
	   "\n"
	   " ## Output options ##\n"
	   " -p           Print details of header, multiple flags can be used\n"
	   " -O           Include offset into file when printing header details\n"
	   " -s           Print a basic summary after processing file(s)\n"
	   " -d           Unpack/decompress data and print the first 6 samples/record\n"
	   " -D           Unpack/decompress data and print all samples\n"
	   "\n"
	   " ## Trace and gap list output options ##\n"
	   " -t           Print a sorted trace list after processing file(s)\n"
	   " -T           Only print a sorted trace list\n"
	   " -tg          Include gap estimates when printing trace list\n"
	   " -tt secs     Specify a time tolerance for continuous traces\n"
	   " -rt diff     Specify a sample rate tolerance for continuous traces\n"
	   " -g           Print a sorted gap/overlap list after processing file(s)\n"
	   " -G           Only print a sorted gap/overlap list\n"
	   " -min secs    Only report gaps/overlaps larger or equal to specified seconds\n"
	   " -max secs    Only report gaps/overlaps smaller or equal to specified seconds\n"
	   " -Q           Additionally group traces by data quality\n"
	   " -H           Heal trace segments, for out of time order data\n"
	   " -tf format   Specify a time string format for trace and gap lists\n"
	   "                format: 0 = SEED time, 1 = ISO time, 2 = epoch time\n"
	   "\n"
	   " ## Data output options ##\n"
	   " -b binfile   Unpack/decompress data and write binary samples to binfile\n"
	   " -o outfile   Write processed records to outfile\n"
	   "\n"
	   " file#        File of Mini-SEED records\n"
	   "\n");
}  /* End of usage() */
