# <p >Mini-SEED Inspector</p>

1. [Name](#)
1. [Synopsis](#synopsis)
1. [Description](#description)
1. [Options](#options)
1. [Input Files](#input-files)
1. [Input List File](#input-list-file)
1. [Match Or Reject List File](#match-or-reject-list-file)
1. [Leap Second List File](#leap-second-list-file)
1. [Author](#author)

## <a id='synopsis'>Synopsis</a>

<pre >
msi [options] file1 [file2 file3 ...]
</pre>

## <a id='description'>Description</a>

<p ><b>msi</b> is a general purpose Mini-SEED parser.  In addition to parsing and displaying the contents of Mini-SEED records, <b>msi</b> will derive continuous trace information for Mini-SEED data.</p>

<p >By default, <b>msi</b> will output a single line of information to summarize each record parsed.  More verbose or different output can be specified via the numerous options.</p>

<p >If '-' is specified standard input will be read.  Multiple input files will be processed in the order specified.</p>

<p >Files on the command line prefixed with a '@' character are input list files and are expected to contain a simple list of input files, see \fBINPUT LIST FILE\fR for more details.</p>

<p >When a input file is full SEED including both SEED headers and data records all of the headers will be skipped and completely unprocessed unless the <b>-y</b> option has been specified.</p>

## <a id='options'>Options</a>

<b>-V</b>

<p style="padding-left: 30px;">Print program version and exit.</p>

<b>-h</b>

<p style="padding-left: 30px;">Print program usage and exit.</p>

<b>-v</b>

<p style="padding-left: 30px;">Be more verbose.  This flag can be used multiple times ("-v -v" or "-vv") for more verbosity.</p>

<b>-r </b><i>reclen</i>

<p style="padding-left: 30px;">Specify the input record length in bytes.  By default the record length of each input Mini-SEED record is automatically detected, this option forces the record length.</p>

<b>-e </b><i>encoding</i>

<p style="padding-left: 30px;">Specify the data encoding format.  These encoding values are the same as those specified in the SEED 1000 Blockette.</p>

<b>-ts </b><i>time</i>

<p style="padding-left: 30px;">Limit processing to Mini-SEED records that contain or start after <i>time</i>.  The format of the <i>time</i> arguement is: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' where valid delimiters are either commas (,), colons (:) or periods (.).</p>

<b>-te </b><i>time</i>

<p style="padding-left: 30px;">Limit processing to Mini-SEED records that contain or end before <i>time</i>.  The format of the <i>time</i> arguement is: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' where valid delimiters are either commas (,), colons (:) or periods (.).</p>

<b>-M </b><i>match</i>

<p style="padding-left: 30px;">Limit processing to Mini-SEED records that match the <i>match</i> regular expression.  For each input record a source name string composed of 'NET_STA_LOC_CHAN_QUAL' is created and compared to the regular expression.  If the match expression begins with an '@' character it is assumed to indicate a file containing a list of expressions to match, see the \fBMATCH OR REJECT LIST FILE\fR section below.</p>

<b>-R </b><i>reject</i>

<p style="padding-left: 30px;">Limit processing to Mini-SEED records that do not match the <i>reject</i> regular expression.  For each input record a source name string composed of 'NET_STA_LOC_CHAN_QUAL' is created and compared to the regular expression.  If the reject expression begins with an '@' character it is assumed to indicate a file containing a list of expressions to reject, see the \fBMATCH OR REJECT LIST FILE\fR section below.</p>

<b>-n </b><i>count</i>

<p style="padding-left: 30px;">Only process <i>count</i> input records.</p>

<b>-y</b>

<p style="padding-left: 30px;">Do not skip non-SEED data records.  By default the program will quietly skip any data which cannot be identified as a SEED data record, allowing the program to work with full SEED volumes and skip bad data.  This option is useful to help identify bad data records.</p>

<b>-p</b>

<p style="padding-left: 30px;">Print details of each record header.  This flag can be used multiple times ("-p -p" or "-pp") for more verbosity.  Specifying two flags will result in all header details being printed.</p>

<b>-O</b>

<p style="padding-left: 30px;">Include the offset into the file in bytes when printing header details.</p>

<b>-L</b>

<p style="padding-left: 30px;">Include data latency when printing header details.  The latency is calcualed as the difference between the time of the last sample and the current time from the host computer.</p>

<b>-s</b>

<p style="padding-left: 30px;">Print a basic summary including the number of records and the number of samples they included after processing all input records.</p>

<b>-d</b>

<p style="padding-left: 30px;">Decompress/unpack data in input records and print the first 6 sample values from each record.</p>

<b>-D</b>

<p style="padding-left: 30px;">Decompress/unpack data in input records and print all the sample values.</p>

<b>-t</b>

<p style="padding-left: 30px;">Print a sorted trace list after processing all input records.</p>

<b>-T</b>

<p style="padding-left: 30px;">Print a sorted trace list after processing all input records and suppress record-by-record output, i.e. trace list only.</p>

<b>-tg</b>

<p style="padding-left: 30px;">Include gap estimates when approriate in trace listings.</p>

<b>-tt </b><i>secs</i>

<p style="padding-left: 30px;">Specify a time tolerance for constructing continous trace segments. The tolerance is specified in seconds.  The default tolerance is 1/2 of the sample period.</p>

<b>-rt </b><i>diff</i>

<p style="padding-left: 30px;">Specify a sample rate tolerance for constructing continous trace segments. The tolerance is specified as the difference between two sampling rates.  The default tolerance is tested as: (abs(1-sr1/sr2) < 0.0001).</p>

<b>-g</b>

<p style="padding-left: 30px;">Print a sorted gap/overlap list after processing all input records.</p>

<b>-G</b>

<p style="padding-left: 30px;">Print a sorted gap/overlap list after processing all input records and suppress record-by-record output, i.e. gap list only.</p>

<b>-gmin </b><i>sec</i>

<p style="padding-left: 30px;">Only include gaps in the gap list larger or equal to <i>sec</i> number of seconds.</p>

<b>-gmax </b><i>sec</i>

<p style="padding-left: 30px;">Only include gaps in the gap list smaller or equal to <i>sec</i> number of seconds.</p>

<b>-S</b>

<p style="padding-left: 30px;">Print a sorted SYNC format trace list after processing all input records and suppress record-by-record output.</p>

<b>-Q</b>

<p style="padding-left: 30px;">Additionally group input data based on SEED data qualitiy flags.  By default data are grouped by network, station, location, channel and adjacent time windows, this option adds data quality to the grouping parameters.</p>

<b>-tf </b><i>format</i>

<p style="padding-left: 30px;">Specify the time stamp format for trace and gap/overlap lists.  The <i>format</i> can be one of the following (default = 0):</p>
<pre style="padding-left: 30px;">
  0 = SEED time, e.g. "2005,068,00:00:01.000000"
  1 = ISO time, e.g. "2005-03-09T00:00:01.000000"
  2 = Epoch seconds, e.g. "1110326401.000000"
</pre>

<b>-b </b><i>binfile</i>

<p style="padding-left: 30px;">Decompress/unpack data in input records and write the binary samples to <i>binfile</i>.</p>

<b>-o </b><i>outfile</i>

<p style="padding-left: 30px;">Write all processed Mini-SEED records to <i>outfile</i>.</p>

## <a id='input-files'>Input Files</a>

<p >An input file name may be followed by an <b>@</b> charater followed by a byte offset.  In this case the program will interpret the byte offset as the starting offset into the file.  Useful for diagnosing specific records.  As an example an input file specified as <b>ANMO.mseed@8192</b> would result in the file <b>ANMO.mseed</b> being read starting at byte 8192.</p>

## <a id='input-list-file'>Input List File</a>

<p >A list file can be used to specify input files, one file per line. The initial '@' character indicating a list file is not considered part of the file name.  As an example, if the following command line option was used:</p>

<pre >
<b>@files.list</b>
</pre>

<p >The 'files.list' file might look like this:</p>

<pre >
data/day1.mseed
data/day2.mseed
data/day3.mseed
</pre>

## <a id='match-or-reject-list-file'>Match Or Reject List File</a>

<p >A list file used with either the <b>-M</b> or <b>-R</b> options contains a list of regular expressions (one on each line) that will be combined into a single compound expression.  The initial '@' character indicating a list file is not considered part of the file name.  As an example, if the following command line option was used:</p>

<pre >
<b>-M @match.list</b>
</pre>

<p >The 'match.list' file might look like this:</p>

<pre >
IU_ANMO_.*
IU_ADK_00_BHZ.*
II_BFO_00_BHZ_Q
</pre>

## <a id='leap-second-list-file'>Leap Second List File</a>

<p >If the environment variable LIBMSEED_LEAPSECOND_FILE is set it is expected to indicate a file containing a list of leap seconds as published by NIST and IETF, usually available here: http://www.ietf.org/timezones/data/leap-seconds.list</p>

<p >If present, the leap seconds listed in this file will be used to adjust the time coverage for records that contain a leap second. Also, leap second indicators in the miniSEED headers will be ignored.</p>

## <a id='author'>Author</a>

<pre >
Chad Trabant
IRIS Data Management Center
</pre>


(man page 2015/03/03)
