/*
 * "$Id$"
 *
 *   IPP test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <errno.h>
#include "ipp.h"
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* WIN32 */


/*
 * Local globals...
 */

int		rpos;				/* Current position in buffer */
ipp_uchar_t	wbuffer[8192];			/* Write buffer */
int		wused;				/* Number of bytes in buffer */
ipp_uchar_t	collection[] =			/* Collection buffer */
		{
		  0x01, 0x01,			/* IPP version */
		  0x00, 0x02,			/* Print-Job operation */
		  0x00, 0x00, 0x00, 0x01,	/* Request ID */
		  IPP_TAG_OPERATION,
		  IPP_TAG_CHARSET,
		  0x00, 0x12,			/* Name length + name */
		  'a','t','t','r','i','b','u','t','e','s','-',
		  'c','h','a','r','s','e','t',
		  0x00, 0x05,			/* Value length + value */
		  'u','t','f','-','8',
		  IPP_TAG_LANGUAGE,
		  0x00, 0x1b,			/* Name length + name */
		  'a','t','t','r','i','b','u','t','e','s','-',
		  'n','a','t','u','r','a','l','-','l','a','n',
		  'g','u','a','g','e',
		  0x00, 0x02,			/* Value length + value */
		  'e','n',
		  IPP_TAG_URI,
		  0x00, 0x0b,			/* Name length + name */
		  'p','r','i','n','t','e','r','-','u','r','i',
		  0x00, 0x1c,			/* Value length + value */
		  'i','p','p',':','/','/','l','o','c','a','l',
		  'h','o','s','t','/','p','r','i','n','t','e',
		  'r','s','/','f','o','o',
		  IPP_TAG_JOB,			/* job group tag */
		  IPP_TAG_BEGIN_COLLECTION,	/* begCollection tag */
		  0x00, 0x09,			/* Name length + name */
		  'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l',
		  0x00, 0x00,			/* No value */
		  IPP_TAG_MEMBERNAME,		/* memberAttrName tag */
		  0x00, 0x00,			/* No name */
		  0x00, 0x0b,			/* Value length + value */
		  'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l', 'o', 'r',
		  IPP_TAG_KEYWORD,		/* keyword tag */
		  0x00, 0x00,			/* No name */
		  0x00, 0x04,			/* Value length + value */
		  'b', 'l', 'u', 'e',
		  IPP_TAG_END_COLLECTION,	/* endCollection tag */
		  0x00, 0x00,			/* No name */
		  0x00, 0x00,			/* No value */
		  IPP_TAG_END			/* end tag */
		};


/*
 * Local functions...
 */

void	hex_dump(ipp_uchar_t *buffer, int bytes);
void	print_attributes(ipp_t *ipp, int indent);
int	read_cb(void *data, ipp_uchar_t *buffer, int bytes);
int	write_cb(void *data, ipp_uchar_t *buffer, int bytes);


/*
 * 'main()' - Main entry.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  ipp_t		*col;		/* Collection */
  ipp_t		*request;	/* Request */
  ipp_state_t	state;		/* State */
  int		length;		/* Length of data */
  int		fd;		/* File descriptor */
  int		i;		/* Looping var */


  request = ippNew();
  request->request.op.version[0]   = 0x01;
  request->request.op.version[1]   = 0x01;
  request->request.op.operation_id = IPP_PRINT_JOB;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, "utf-8");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, "en");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, "ipp://localhost/printers/foo");

  col = ippNew();
  ippAddString(col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-color", NULL, "blue");
  ippAddCollection(request, IPP_TAG_JOB, "media-col", col);

  length = ippLength(request);
  if (length != sizeof(collection))
    printf("ERROR ippLength didn't compute the correct length (%d instead of %d bytes!)\n",
           length, sizeof(collection));

  wused = 0;
  while ((state = ippWriteIO(wbuffer, write_cb, 1, NULL, request)) != IPP_DATA)
    if (state == IPP_ERROR)
      break;

  if (state != IPP_DATA)
    puts("ERROR writing collection attribute!");

  printf("%d bytes written:\n", wused);
  hex_dump(wbuffer, wused);

  if (wused != sizeof(collection))
  {
    printf("ERROR expected %d bytes!\n", sizeof(collection));
    hex_dump(collection, sizeof(collection));
  }
  else if (memcmp(wbuffer, collection, wused))
  {
    puts("ERROR output does not match baseline!");
    hex_dump(collection, sizeof(collection));
  }

  ippDelete(col);
  ippDelete(request);

  request = ippNew();
  rpos    = 0;

  while ((state = ippReadIO(wbuffer, read_cb, 1, NULL, request)) != IPP_DATA)
    if (state == IPP_ERROR)
      break;

  if (state != IPP_DATA)
    puts("ERROR reading collection attribute!");

  printf("%d bytes read.\n", rpos);

  puts("Core IPP tests passed.");

  for (i = 1; i < argc; i ++)
  {
    if ((fd = open(argv[i], O_RDONLY)) < 0)
    {
      printf("Unable to open \"%s\" - %s\n", argv[i], strerror(errno));
      continue;
    }

    request = ippNew();
    while ((state = ippReadFile(fd, request)) == IPP_ATTRIBUTE);

    if (state != IPP_DATA)
      printf("Error reading IPP message from \"%s\"!\n", argv[i]);
    else
    {
      printf("\n%s:\n", argv[i]);
      print_attributes(request, 4);
    }

    ippDelete(request);
    close(fd);
  }

  return (0);
}


/*
 * 'hex_dump()' - Produce a hex dump of a buffer.
 */

void
hex_dump(ipp_uchar_t *buffer,		/* I - Buffer to dump */
         int         bytes)		/* I - Number of bytes */
{
  int	i, j;				/* Looping vars */
  int	ch;				/* Current ASCII char */


 /*
  * Show lines of 16 bytes at a time...
  */

  for (i = 0; i < bytes; i += 16)
  {
   /*
    * Show the offset...
    */

    printf("%04x ", i);

   /*
    * Then up to 16 bytes in hex...
    */

    for (j = 0; j < 16; j ++)
      if ((i + j) < bytes)
        printf(" %02x", buffer[i + j]);
      else
        printf("   ");

   /*
    * Then the ASCII representation of the bytes...
    */

    putchar(' ');
    putchar(' ');

    for (j = 0; j < 16 && (i + j) < bytes; j ++)
    {
      ch = buffer[i + j] & 127;

      if (ch < ' ' || ch == 127)
        putchar('.');
      else
        putchar(ch);
    }

    putchar('\n');
  }
}


/*
 * 'print_attributes()' - Print the attributes in a request...
 */

void
print_attributes(ipp_t *ipp,		/* I - IPP request */
                 int   indent)		/* I - Indentation */
{
  int			i;		/* Looping var */
  ipp_tag_t		group;		/* Current group */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_value_t		*val;		/* Current value */
  static const char * const tags[] =	/* Value/group tag strings */
			{
			  "reserved-00",
			  "operation-attributes-tag",
			  "job-attributes-tag",
			  "end-of-attributes-tag",
			  "printer-attributes-tag",
			  "unsupported-attributes-tag",
			  "subscription-attributes-tag",
			  "event-attributes-tag",
			  "reserved-08",
			  "reserved-09",
			  "reserved-0A",
			  "reserved-0B",
			  "reserved-0C",
			  "reserved-0D",
			  "reserved-0E",
			  "reserved-0F",
			  "unsupported",
			  "default",
			  "unknown",
			  "no-value",
			  "reserved-14",
			  "not-settable",
			  "delete-attr",
			  "admin-define",
			  "reserved-18",
			  "reserved-19",
			  "reserved-1A",
			  "reserved-1B",
			  "reserved-1C",
			  "reserved-1D",
			  "reserved-1E",
			  "reserved-1F",
			  "reserved-20",
			  "integer",
			  "boolean",
			  "enum",
			  "reserved-24",
			  "reserved-25",
			  "reserved-26",
			  "reserved-27",
			  "reserved-28",
			  "reserved-29",
			  "reserved-2a",
			  "reserved-2b",
			  "reserved-2c",
			  "reserved-2d",
			  "reserved-2e",
			  "reserved-2f",
			  "octetString",
			  "dateTime",
			  "resolution",
			  "rangeOfInteger",
			  "begCollection",
			  "textWithLanguage",
			  "nameWithLanguage",
			  "endCollection",
			  "reserved-38",
			  "reserved-39",
			  "reserved-3a",
			  "reserved-3b",
			  "reserved-3c",
			  "reserved-3d",
			  "reserved-3e",
			  "reserved-3f",
			  "reserved-40",
			  "textWithoutLanguage",
			  "nameWithoutLanguage",
			  "reserved-43",
			  "keyword",
			  "uri",
			  "uriScheme",
			  "charset",
			  "naturalLanguage",
			  "mimeMediaType",
			  "memberName"
			};


  for (group = IPP_TAG_ZERO, attr = ipp->attrs; attr; attr = attr->next)
  {
    if (attr->group_tag == IPP_TAG_ZERO || !attr->name)
    {
      group = IPP_TAG_ZERO;
      putchar('\n');
      continue;
    }

    if (group != attr->group_tag)
    {
      group = attr->group_tag;

      putchar('\n');
      for (i = 2; i < indent; i ++)
        putchar(' ');

      printf("%s:\n\n", tags[group]);
    }

    for (i = 0; i < indent; i ++)
      putchar(' ');

    printf("%s (%s):", attr->name, tags[attr->value_tag]);

    switch (attr->value_tag)
    {
      case IPP_TAG_ENUM :
      case IPP_TAG_INTEGER :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %d", val->integer);
          putchar('\n');
          break;

      case IPP_TAG_BOOLEAN :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %s", val->boolean ? "true" : "false");
          putchar('\n');
          break;

      case IPP_TAG_RANGE :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %d-%d", val->range.lower, val->range.upper);
          putchar('\n');
          break;

      case IPP_TAG_DATE :
          {
	    time_t	vtime;		/* Date/Time value */
	    struct tm	*vdate;		/* Date info */
	    char	vstring[256];	/* Formatted time */

	    for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    {
	      vtime = ippDateToTime(val->date);
	      vdate = localtime(&vtime);
	      strftime(vstring, sizeof(vstring), "%c", vdate);
	      printf(" (%s)", vstring);
	    }
          }
          putchar('\n');
          break;

      case IPP_TAG_RESOLUTION :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" %dx%d%s", val->resolution.xres, val->resolution.yres,
	           val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpc");
          putchar('\n');
          break;

      case IPP_TAG_STRING :
      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    printf(" \"%s\"", val->string.text);
          putchar('\n');
          break;

      case IPP_TAG_BEGIN_COLLECTION :
          putchar('\n');

          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    print_attributes(val->collection, indent + 4);
          break;

      default :
          putchar('\n');
          break;
    }

  }
}


/*
 * 'read_cb()' - Read data from a buffer.
 */

int					/* O - Number of bytes read */
read_cb(void        *data,		/* I - Data */
        ipp_uchar_t *buffer,		/* O - Buffer to read */
	int         bytes)		/* I - Number of bytes to read */
{
  int	count;				/* Number of bytes */


 /*
  * Copy bytes from the data buffer to the read buffer...
  */

  for (count = bytes; count > 0 && rpos < wused; count --, rpos ++)
    *buffer++ = wbuffer[rpos];

 /*
  * Return the number of bytes read...
  */

  return (bytes - count);
}


/*
 * 'write_cb()' - Write data into a buffer.
 */

int					/* O - Number of bytes written */
write_cb(void        *data,		/* I - Data */
         ipp_uchar_t *buffer,		/* I - Buffer to write */
	 int         bytes)		/* I - Number of bytes to write */
{
  int	count;				/* Number of bytes */


 /*
  * Loop until all bytes are written...
  */

  for (count = bytes; count > 0 && wused < sizeof(wbuffer); count --, wused ++)
    wbuffer[wused] = *buffer++;

 /*
  * Return the number of bytes written...
  */

  return (bytes - count);
}


/*
 * End of "$Id$".
 */
