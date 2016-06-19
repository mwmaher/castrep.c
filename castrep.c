/* CastRep.C

   By Michael W. Maher

   Ver 1.0 4/16/91

   The following program is a simple report generator.  It reads two
   ASCII files of sorted records to accumulate, then formats a report.

   The files originate from PLATNUM software.  The first file contains
   the part number, location, division, and number of castings.  The second
   file contains the part number and the weight per casting.
*/

/* standard MCS 5.0 header files */
#include<conio.h>
#include<ctype.h>
#include<float.h>
#include<io.h>
#include<math.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<time.h>

/* Blaise Computing C TOOLS PLUS 6.0 */
#include <bstrings.h>

/* definitions */
/* #define DEBUG_SET */
/* #define DEBUG_REPORT2_SET */

#define TRUE 1
#define FALSE 0

#define OK 0
#define UNABLE_TO_OPEN 1
#define LOCATION_ERROR 2
#define EOF_FOUND 3
#define DONE 4
#define FOUND 5

#define RECORD_SIZE 45
#define PART_SIZE 18
#define LOC_SIZE 5
#define DIV_SIZE 3
#define AMOUNT_SIZE (RECORD_SIZE - PART_SIZE - LOC_SIZE - DIV_SIZE)
#define WEIGHT_SIZE (34 - PART_SIZE)
#define MAX_DIVS 5
#define MAX_LOCATIONS 4
#define LEFT_MARGIN 5
#define PGWIDTH 80

/* structures */
typedef struct item_rec
  {
  char  part[PART_SIZE+1];
  long  loc_amount[4];
  float weight;
  } item_rec;

typedef struct total_rec
  {
  char  div[DIV_SIZE+1];
  char  div_name[15];
  float loc_weight[4];
  } total_rec;

/* globals */
total_rec totals[MAX_DIVS];
FILE      *main_file_ptr,
	  *weight_file_ptr,
	  *out;

/* function prototypes */
int  main(int artc, char **argv);
int do_report1(char *in_filename,
	       char *weight_filename,
	       char *div,
	       FILE *out);
int  do_report2(total_rec *totals, char *div, FILE *out);
void print_header(int *lns,
		  int* pgs,
		  char *div,
		  char *title,
		  char *label,
		  FILE *out);
char get_next_item(FILE *main_file_ptr,
		   FILE *weight_file_ptr,
		   item_rec *item,
		   long *record);
int  parse_rec(item_rec *rec, char *str_rec, char *div);
void print_item(item_rec *item, int *lns, FILE *out);
void init_item(item_rec *item);
void total_by_div(char *div, item_rec *rec, total_rec *totals);
void init_totals(total_rec *totals);
void set_weight(item_rec *rec, FILE *div_file_ptr);

/* program */
int main(int argc, char **argv)
  {
  char *division_name = "Machining Division";
  int  error = 0;

  init_totals(totals);
  if (argc > 2)
    {
    if (strcmpi("/p", argv[3]) == 0)
      out = stdprn;
    else
      out = stdout;
    error = do_report1(argv[1], argv[2], division_name, out);
    if (error == 0)
      error = do_report2(totals, division_name, out);
    }
  else
    {
    printf("\nCastRep - by Michael W. Maher for The Metalloy Corporation"
	   "\nCastRep is a casting inventory report generator designed to"
	   "\nbe used with the PLATNIUM software.\n"
	   "\nUSAGE: castrep [d:][path]filespec [d:][path]filespec [/p]\n"
	   "\nThe first filespec is the main data file, second is the item"
	   "\nweight data file.  The /p option will send the output to the "
	   "printer.");
    }
  return(error);
  }


int do_report1(char *in_filename,
	       char *weight_filename,
	       char *div,
	       FILE *out)
  {
  char     flag,
	   label[81];
  int      error = 0,
	   lns = 0,
	   pgs = 0,
	   parts = 0;
  long     record = 0L;
  item_rec item;

  sprintf(label,"%-*s %12s %8s %8s %8s %8s\n",
	  PART_SIZE,
	  "Part Number",
	  "Total",
	  "Raw",
	  "Outside",
	  "Process",
	  "Ship");
  if (((main_file_ptr = fopen(in_filename, "rt")) != NULL) &&
      ((weight_file_ptr = fopen(weight_filename, "rt")) != NULL))
    {
    print_header(&lns, &pgs, div, "Casting Inventory Report", label, out);
    do
      {
      init_item(&item);
      flag = get_next_item(main_file_ptr, weight_file_ptr, &item, &record);
      if (flag == FOUND)
	{
	parts += 1;
	print_item(&item, &lns, out);
	if (lns > 55)
	  print_header(&lns, &pgs, div, "Casting Inventory Report",label,out);
	}
      }
    while (flag == FOUND);
    fprintf(out, "%*sEND OF REPORT.  %d parts processed\f",
	    LEFT_MARGIN, " ",  parts);
    fclose(main_file_ptr);
    fclose(weight_file_ptr);
    }
  else
    {
    error = UNABLE_TO_OPEN;
    printf("\nERROR: unable to open [%s] and/or [%s].\n",
	   in_filename, weight_filename);
    }
  return(error);
  }


void print_header(int *lns,
		  int* pgs,
		  char *div,
		  char *title,
		  char *label,
		  FILE *out)
  {
  char buf[9];

  if (*pgs > 0)
    fprintf(out, "\f\n");
  *pgs += 1;
  fprintf(out,"\n%*spage %-d\n", PGWIDTH - 10, " ", *pgs);
  fprintf(out,"%*s%s\n", (int)(PGWIDTH - strlen(div))/2, " ", div);
  fprintf(out,"%*s%s\n", (int)(PGWIDTH - strlen(title))/2, " ", title);
  fprintf(out,"%*s-%8s-\n\n", (int)(PGWIDTH - 10)/2, " ", _strdate(buf));
  fprintf(out,"%*s%s\n", LEFT_MARGIN, " ", label);
  *lns = 9;
  return;
  }


char get_next_item(FILE *main_file_ptr,
		   FILE *weight_file_ptr,
		   item_rec *item,
		   long *record)
  {
  char     div[DIV_SIZE + 1],
	   new_flag = TRUE,
	   loop_flag = TRUE,
	   buf[RECORD_SIZE + 1];
  item_rec temp;
  int      i;

  do
    {
    strset(buf, '\0');
    fseek(main_file_ptr, (*record) * RECORD_SIZE, SEEK_SET);
    if (fread(buf, (long)sizeof(char),RECORD_SIZE,main_file_ptr)>=RECORD_SIZE-2)
      {
      buf[RECORD_SIZE] = '\0';
      buf[RECORD_SIZE - (1*sizeof(char))] = '\0';
      buf[RECORD_SIZE - (2*sizeof(char))] = '\0';
      #ifdef DEBUG_SET
	printf("BUFFER [%*s]", (int) RECORD_SIZE, buf);
      #endif
      init_item(&temp);
      parse_rec(&temp, buf, div);
      if (new_flag == TRUE)
	{
	#ifdef DEBUG_SET
	   for (i = 0; i < MAX_LOCATIONS; i++)
	     printf("0 intermediate amount[%d] = %ld  temp = %ld\n",
		     i, item->loc_amount[i], temp.loc_amount[i]);
	#endif
	set_weight(&temp, weight_file_ptr);
	*record += 1;
	strncpy(item->part, temp.part, PART_SIZE);
	for (i = 0; i < MAX_LOCATIONS; i++)
	  item->loc_amount[i] += temp.loc_amount[i];
	total_by_div(div, &temp, totals);
	new_flag = FALSE;
	#ifdef DEBUG_SET
	  for (i = 0; i < MAX_LOCATIONS; i++)
	    printf("1 intermediate amount[%d] = %ld  temp = %ld\n",
		    i, item->loc_amount[i], temp.loc_amount[i]);
	#endif
	}
      else if (strcmp(temp.part, item->part) == 0)
	{
	#ifdef DEBUG_SET
	  for (i = 0; i < MAX_LOCATIONS; i++)
	    printf("1.5 intermediate amount[%d] = %ld  temp = %ld\n",
		   i, item->loc_amount[i], temp.loc_amount[i]);
	#endif
	*record += 1;
	for (i = 0; i < MAX_LOCATIONS; i++)
	  item->loc_amount[i] += temp.loc_amount[i];
	#ifdef DEBUG_SET
	  for (i = 0; i < MAX_LOCATIONS; i++)
	    printf("2 intermediate amount[%d] = %ld\n", i, item->loc_amount[i]);
	#endif
	total_by_div(div, &temp, totals);
	}
      else
	loop_flag = FALSE;
      }
    else
      loop_flag = FALSE;
    }
  while (loop_flag == TRUE);
  #ifdef DEBUG_SET
    for (i = 0; i < MAX_LOCATIONS; i++)
      printf("final amount[%d] = %ld\n", i, item->loc_amount[i]);
  #endif
  if (new_flag == TRUE)
    loop_flag = DONE;
  else
    loop_flag = FOUND;
  return(loop_flag);
  }


int parse_rec(item_rec *rec, char *str_rec, char *div)
  {
  char temp[RECORD_SIZE + 1];
  int  index,
       i;

  /* parse the part */
  strncpy(rec->part, str_rec, PART_SIZE);
  rec->part[PART_SIZE] = '\0';
  stpcvt(rec->part, RWHITE | RCONTROL);
  #ifdef DEBUG_SET
    printf("PART-%s\n", rec->part);
  #endif

  /* parse the location 1, 2, 3, 4 */
  index = (int) str_rec[19] - '1';
  #ifdef DEBUG_SET
    printf("INDEX = %d\n", index);
  #endif
  if ((index >= MAX_LOCATIONS) || (index < 0))
    {
    printf("\n\aERROR: location character [%c] invalid.\n", index + '1');
    printf("Strike any key to continue.\n");
    getch();
    return(LOCATION_ERROR);
    }

  /* parse the division */
  for (i = 0; i < DIV_SIZE; i++)
    div[i] = str_rec[i + 23];
  div[i] = '\0';

  /* parse the amount and convert to a long */
  strset(temp, '\0');
  for (i = 0; i < AMOUNT_SIZE; i++)
    temp[i] = str_rec[PART_SIZE + LOC_SIZE + DIV_SIZE + i];
  temp[AMOUNT_SIZE] = '\0';
  rec->loc_amount[index] = atol(temp);
  #ifdef DEBUG_SET
    if (index == 0)
      {
      printf("amount [0] %ld  temp [%s]\n", atol(temp), temp);
      rec->loc_amount[0] = atol(temp);
      printf("AMOUNT[%d] = %ld\n", index, rec->loc_amount[index]);
      }
  #endif
  #ifdef DEBUG_SET
    printf("AMOUNT[%d] = %ld\n", index, rec->loc_amount[index]);
  #endif
  return(OK);
  }

void init_item(item_rec *item)
  {
  int i;

  strset(item->part, '\0');
  for (i = 0; i < MAX_LOCATIONS; i++)
   item->loc_amount[i] = 0L;
  return;
  }


void print_item(item_rec *item, int *lns, FILE *out)
  {
  int  i;
  long total = 0L;

  *lns += 1;
  for (i = 0; i < MAX_LOCATIONS; i++)
    total += item->loc_amount[i];
  fprintf(out,"%*s%-*s %12ld %8ld %8ld %8ld %8ld\n",
	  LEFT_MARGIN, " ",
	  PART_SIZE,
	  item->part,
	  total,
	  item->loc_amount[0],
	  item->loc_amount[1],
	  item->loc_amount[2],
	  item->loc_amount[3]);
  return;
  }

/***************************Total weight report*****************************/

void init_totals(total_rec *totals)
  {
  int i, j;

  strcpy(totals[0].div, "ZMF");
  strcpy(totals[0].div_name, "MI Foundry");
  strcpy(totals[1].div, "ZMS");
  strcpy(totals[1].div_name, "MS Foundry");
  strcpy(totals[2].div, "ZMD");
  strcpy(totals[2].div_name, "MI Diecast");
  strcpy(totals[3].div, "ZIN");
  strcpy(totals[3].div_name, "IN Diecast");
  strcpy(totals[4].div, "ZMM");
  strcpy(totals[4].div_name, "MI Machining");
  for (i = 0; i < MAX_DIVS; i++)
    {
    for (j = 0; j < MAX_LOCATIONS; j++)
      totals[i].loc_weight[j] = 0.0f;
    }
  return;
  }


void total_by_div(char *div, item_rec *rec, total_rec *totals)
  {
  int i, j;

  for (i = 0; ((i < MAX_DIVS) && (strcmp(div, totals[i].div) != 0)); i++);
  if (i < MAX_DIVS)
    {
    for (j = 0; j < MAX_LOCATIONS; j++)
      totals[i].loc_weight[j] += (rec->weight * (float)rec->loc_amount[j]);
    }
  else
    {
    printf("\nERROR: Division code [%s] invalid.", div);
    printf("\nStrike any key to continue.\n");
    getch();
    }
  #ifdef DEBUG_REPORT2_SET
    printf("done i = %d div %s div comp %s\n", i, div, totals[i].div);
    for (j = 0; j < MAX_LOCATIONS; j++)
      printf("%d loc_weight[%d] %.2f = %.2f %.2f\n",
	     i, j,
	     totals[i].loc_weight[j],
	     rec->weight, (float)rec->loc_amount[j]);
  #endif
  return;
  }


void set_weight(item_rec *rec, FILE *div_file_ptr)
  {
  char part_str[PART_SIZE + 1],
       weight_str[WEIGHT_SIZE + 1],
       error = FALSE;

  do
    {
    strset(part_str, '\0');
    strset(weight_str, '\0');
    if (fread(part_str, (long) PART_SIZE, 1, div_file_ptr) >= 1)
      {
      part_str[PART_SIZE] = '\0';
      stpcvt(part_str, RWHITE | RCONTROL);
      if (fread(weight_str, (long) WEIGHT_SIZE, 1, div_file_ptr) < 1)
	error = TRUE;
      weight_str[WEIGHT_SIZE] = '\0';
      weight_str[WEIGHT_SIZE - sizeof(char)] = '\0';
      }
    else
      error = TRUE;
    #ifdef DEBUG_REPORT2_SET
      printf("Search r[%s] p[%s] wstr[%s] w[%.2f]\n",
	      rec->part, part_str, weight_str, (float) atof(weight_str));
    #endif
    }
  while ((strcmp(part_str, rec->part) != 0) && (error != TRUE));
  if (error == FALSE)
    {
    weight_str[WEIGHT_SIZE] = '\0';
    rec->weight = (float) atof(weight_str);
    }
  else
    {
    fseek(div_file_ptr, 0L, SEEK_SET);
    printf("\n\aERROR: part [%s] not found in weight file", rec->part);
    printf("\nAny key to continue.\n");
    rec->weight = 0.0f;
    getch();
    }
  return;
  }


int  do_report2(total_rec *totals, char *div, FILE *out)
  {
  int   lns = 0,
	pgs = 0,
	i, j;
  char  label[81];
  float total_weight;

  sprintf(label, "%12s %11s %10s %10s %10s %10s",
	  "Division",
	  "Total lbs",
	  "Raw",
	  "Outside",
	  "In Process",
	  "To Ship");
  print_header(&lns, &pgs,
	       div,
	       "Total Casting Weight By Division",
	       label,
	       out);
  for (i = 0; i < MAX_DIVS; i++)
    {
    total_weight = 0.0f;
    for (j = 0; j < MAX_LOCATIONS; j++)
      total_weight += totals[i].loc_weight[j];
    fprintf(out, "%*s%12s %11.0f %10.0f %10.0f %10.0f %10.0f\n\n",
	    LEFT_MARGIN, " ",
	    totals[i].div_name,
	    total_weight,
	    totals[i].loc_weight[0],
	    totals[i].loc_weight[1],
	    totals[i].loc_weight[2],
	    totals[i].loc_weight[3]);
    }
  fprintf(out, "\f");
  return(0);
  }


