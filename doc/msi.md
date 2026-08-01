# miniSEED Inspector

1. [Synopsis](#synopsis)
1. [Description](#description)
1. [Options](#options)
1. [Input Files](#input-files)
1. [Input List File](#input-list-file)
1. [Leap Second List File](#leap-second-list-file)
1. [Author](#author)

## <a id="synopsis">Synopsis</a>

```
msi [options] file1 [file2 file3 ...]

```

## <a id="description">Description</a>

<b>msi</b> is a general purpose miniSEED parser.  In addition to parsing and displaying the contents of miniSEED records, <b>msi</b> will derive continuous trace information for miniSEED data.

By default, <b>msi</b> will output a single line of information to summarize each record parsed.  More verbose or different output can be specified via the numerous options.

If '-' is specified standard input will be read.  Multiple input files will be processed in the order specified.

Files on the command line prefixed with a '@' character are input list files and are expected to contain a simple list of input files, see \fBINPUT LIST FILE\fR for more details.

When an input file contains data that cannot be identified as miniSEED, such as full SEED headers, <b>msi</b> will stop processing that file unless the <b>-snd</b> option has been specified, in which case the unidentified data are skipped.

## <a id="options">Options</a>

- <b>-V</b>
  Print program version and exit.

- <b>-h</b>
  Print program usage and exit.

- <b>-v</b>
  Be more verbose.  This flag can be used multiple times ("-v -v" or "-vv") for more verbosity.

- -H <i>Header</i>
  Add custom header to URL-based reads.  Only available when compiled with URL support.

- -u <i>user:pass</i>
  Set <i>user</i> and <i>pass</i> credentials for URL-based reads.  Only available when compiled with URL support.

- -ts <i>time</i>
  Limit processing to miniSEED records that contain or start after <i>time</i>.  The format of the <i>time</i> arguement is: 'YYYY[-MM-DDThh:mm:ss.ffff], or 'YYYY[,DDD,HH,MM,SS,FFFFFF]', or Unix/POSIX epoch seconds.

- -te <i>time</i>
  Limit processing to miniSEED records that contain or end before <i>time</i>.  The format of the <i>time</i> arguement is: 'YYYY[-MM-DDThh:mm:ss.ffff], or 'YYYY[,DDD,HH,MM,SS,FFFFFF]', or Unix/POSIX epoch seconds.

- -m <i>match</i>
  Limit processing to miniSEED records that contain the <i>match</i> pattern, which is applied to the Source Identifier for each record, often following this pattern: 'FDSN:&lt;network&gt;_&lt;station&gt;_&lt;location&gt;_&lt;band&gt;_&lt;source&gt;_&lt;subsource&gt;' The pattern is a glob expression: '*' matches zero or more characters, '?' matches any single character, and '[set]', '[!set]' or '[^set]' match or exclude any character in the set. May be used multiple times, a record matching any pattern is kept.

- -r <i>reject</i>
  Limit processing to miniSEED records that do _not_ contain the <i>reject</i> pattern, which is applied to the the Source Identifier for each record, often following this pattern: 'FDSN:&lt;network&gt;_&lt;station&gt;_&lt;location&gt;_&lt;band&gt;_&lt;source&gt;_&lt;subsource&gt;' The pattern supports the same glob syntax as <b>-m</b>. May be used multiple times, a record matching any pattern is rejected.

- -n <i>count</i>
  Only process <i>count</i> input records.

- <b>-snd</b>
  Skip non-miniSEED records.  By default the program will stop when it encounters data that cannot be identified as a miniSEED record. This option can be useful with full SEED volumes or files with bad data.

- <b>-p</b>
  Print details of each record header.  This flag can be used multiple times ("-p -p" or "-pp") for more verbosity.  Specifying two flags will result in all header details being printed.

- <b>-O</b>
  Include the offset into the file in bytes when printing header details.

- <b>-L</b>
  Include data latency when printing header details.  The latency is calculated as the difference between the time of the last sample and the current time from the host computer.

- <b>-s</b>
  Print a basic summary including the number of records and the number of samples they included after processing all input records.

- <b>-d</b>
  Decompress/unpack data in input records and print the first 6 sample values from each record.

- <b>-D</b>
  Decompress/unpack data in input records and print all the sample values.

- <b>-z</b>
  Validate and print record header details in a raw, unparsed form instead of the normal parsed form.  Combine with <b>-p</b> to control the level of detail.

- <b>-t</b>
  Print a sorted trace list after processing all input records.

- <b>-T</b>
  Print a sorted trace list after processing all input records and suppress record-by-record output, i.e. trace list only.

- <b>-tg</b>
  Include gap estimates when approriate in trace listings and suppress record-by-record output, i.e. trace list only.

- -tt <i>secs</i>
  Specify a time tolerance for constructing continous trace segments. The tolerance is specified in seconds.  The default tolerance is 1/2 of the sample period.

- -rt <i>diff</i>
  Specify a sample rate tolerance for constructing continous trace segments. The tolerance is specified as the difference between two sampling rates.  The default tolerance is tested as: (abs(1-sr1/sr2) &lt; 0.0001).

- <b>-g</b>
  Print a sorted gap/overlap list after processing all input records.

- <b>-G</b>
  Print a sorted gap/overlap list after processing all input records and suppress record-by-record output, i.e. gap list only.

- -gmin <i>sec</i>
  Only include gaps in the gap list larger or equal to <i>sec</i> number of seconds.

- -gmax <i>sec</i>
  Only include gaps in the gap list smaller or equal to <i>sec</i> number of seconds.

- <b>-S</b>
  Print a sorted SYNC format trace list after processing all input records and suppress record-by-record output.

- <b>-P</b>
  Additionally group input data by publication.  Note: for miniSEED version 2 records, SEED data qualitiy values are translated to publication versions. By default data are grouped by network, station, location, channel and adjacent time windows, this option adds publication version to the grouping parameters.

- -tf <i>format</i>
  Specify the time stamp format for trace and gap/overlap lists.  The <i>format</i> can be one of the following (default = 0):

  ```
    0 = SEED day-of-year time, e.g. "2005,068,00:00:01.000000"
    1 = ISO year-month-day time, e.g. "2005-03-09T00:00:01.000000"
    2 = Epoch seconds, e.g. "1110326401.000000"
  ```

- -b <i>binfile</i>
  Decompress/unpack data in input records and write the binary samples to <i>binfile</i>.

- -o <i>outfile</i>
  Write all processed miniSEED records to <i>outfile</i>.

## <a id="input-files">Input Files</a>

An input file name may be followed by an <b>@</b> charater followed by a byte range in the pattern <b>START[-END]</b>, where the END offset is optional.  As an example an input file specified as <b>ANMO.mseed@8192</b> would result in the file <b>ANMO.mseed</b> being read starting at byte 8192.  An optional end offset can be specified, e.g. <b>ANMO.mseed@8192-12288</b> would start reading at offset 8192 and stop after offset 12288.

## <a id="input-list-file">Input List File</a>

A list file can be used to specify input files, one file per line. The initial '@' character indicating a list file is not considered part of the file name.  As an example, if the following command line option was used:

```
@files.list
```

The 'files.list' file might look like this:

```
data/day1.mseed
data/day2.mseed
data/day3.mseed
```

## <a id="leap-second-list-file">Leap Second List File</a>

NOTE: A list of leap seconds is included in the program and no external list should be needed unless a leap second is added after year 2023.

If the environment variable LIBMSEED_LEAPSECOND_FILE is set it is expected to indicate a file containing a list of leap seconds as published by NIST and IETF, usually available here: https://www.ietf.org/timezones/data/leap-seconds.list

If present, the leap seconds listed in this file will be used to adjust the time coverage for records that contain a leap second. Also, leap second indicators in the miniSEED headers will be ignored.

## <a id="author">Author</a>

```
Chad Trabant
EarthScope Data Services
```

---

*Generated from man page dated 2026/08/01.*
