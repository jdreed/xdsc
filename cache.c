/*
**  cache.c:  manage the files in /tmp
**
**  Ensure we have the ones we need, and get rid of the ones no
**  longer in use.
**
*/

#include	<stdio.h>
#include	<X11/Intrinsic.h>
#include	<X11/StringDefs.h>
#include	<X11/IntrinsicP.h>
#include	<X11/CoreP.h>
#include	<X11/Xaw/Command.h>
#include	<X11/Xaw/AsciiText.h>
#include	<X11/Xaw/TextP.h>
#include	"xdsc.h"

#define		NUM_CACHED_FILES	5

static char rcsid[] = "$Header: /afs/dev.mit.edu/source/repository/athena/bin/xdsc/cache.c,v 1.7 1991-03-12 16:33:51 sao Exp $";

extern char	*RunCommand();
extern EntryRec		toplevelbuttons[2][MAX_BUTTONS];
extern TextWidget	bottextW, toptextW;
extern Boolean	debug;
extern char	filebase[];
extern int	topscreen;
extern int	edscversion;
extern Widget	topW;
extern void	TopSelect();

extern char	axis[];

static int	next=0, prev=0, nref=0;
static int	pref=0, fref=0, lref=0;
static int	first=0, last=0;
static int	highestseen;
static int	current;

static int	cache[NUM_CACHED_FILES] = {-1,-1,-1,-1,-1};
static char	currentmtglong[LONGNAMELEN] = "";
static char	currentmtgshort[SHORTNAMELEN] = "";

char *
CurrentMtg(which)
int	which;
{
	if (which == 0)
		return (currentmtglong);
	else
		return (currentmtgshort);
}

int
SaveMeetingNames(newlong, newshort)
char	*newlong;
char	*newshort;
{
	char	oldlong[LONGNAMELEN];
	char	oldshort[SHORTNAMELEN];
	if (*currentmtglong) {
		MarkLastRead();
		DeleteOldTransactions();
	}

	if (*newlong) {
		strcpy (oldlong, currentmtglong);
		strcpy (currentmtglong, newlong);
		strcpy (oldshort, currentmtgshort);
		strcpy (currentmtgshort, newshort);
		if (SetUpTransactionNumbers() == -1) {
			strcpy (currentmtglong, oldlong);
			strcpy (currentmtgshort, oldshort);
			return (-1);
		}
	}

	else {
		*currentmtglong = '\0';
		*currentmtgshort = '\0';
	}
	return (0);
}

/*
**  GoToTransaction updates the data structures and cache so that
**  we are in a particular transaction.
**
**  if 'update' is True (it usually is) we put the contents of the
**  transaction into the lower text widget.
*/

GoToTransaction (num, update)
int	num;
Boolean	update;
{
	static char	command[LONGNAMELEN + 25];
	static char	filename[70];
	char		*returndata;

	if (!num) return;

	sprintf (filename, "%s-%d", filebase, num);

	if (update && topscreen == LISTTRNS);
		UpdateHighlightedTransaction(num);

	sprintf (command, "Reading %s [%d-%d], #%d...", 
				currentmtglong, first, last, num);
	PutUpStatusMessage(command);

	if (GetTransactionFile(num) == -1)
		return (-1);

	if (update)
		FileIntoWidget(filename, bottextW);

	sprintf (command, "(gti %d %s)\n", num, currentmtglong);
	returndata = RunCommand (command, NULL, NULL, True);

	if ((int) returndata <= 0) goto DONE;

	sscanf (	returndata, 
			"%*c%d%d%d%d%d%d%d%*s",
			&current, &prev, &next, &pref, 
			&nref, &fref, &lref);

	myfree(returndata);
	CheckButtonSensitivity(BUTTONS_UPDATE);

	CacheSurroundingTransactions();

	if ( current >  highestseen)
		highestseen = current;

	if ( next > last || current > last) {
		MarkLastRead();
		(void) SetUpTransactionNumbers();
	}

	if (highestseen == last && topscreen == MAIN)
		RemoveLetterC();
DONE:
	sprintf (command, "Reading %s [%d-%d], #%d", 
				currentmtglong, first, last, num);
	PutUpStatusMessage(command);
}

DeleteOldTransactions()
{
	char		filename[80];
	unsigned int	fd;

	DeleteTransactionFile(current);
	DeleteTransactionFile(prev);
	DeleteTransactionFile(next);
	DeleteTransactionFile(pref);
	DeleteTransactionFile(nref);

	sprintf (filename, "%s-list", filebase);
	if ((fd = open(filename, O_RDONLY)) != -1) {
		close (fd);
		unlink (filename);
	}

	next=0, prev=0, nref=0;
	pref=0, fref=0, lref=0;
	first=0, last=0, current=0;
}

/*
** Read in the transactions immedidately before and after the
** current one.
*/

CacheSurroundingTransactions()
{

	int		i;

/*
** Remove unnecessary files
*/

	for (i = 0; i < NUM_CACHED_FILES; i++) {
		if (	cache[i] != next && cache[i] != prev &&
			cache[i] != nref && cache[i] != pref &&
			cache[i] != current && cache[i] != -1) {

			DeleteTransactionFile(cache[i]);
		}
	}

/*
** Get the files we need
*/
	(void) GetTransactionFile (next);
	(void) GetTransactionFile (prev);
	(void) GetTransactionFile (nref);
	(void) GetTransactionFile (pref);
	(void) GetTransactionFile (current);

}

/*
** Remove the file from /tmp and the cache.
*/

DeleteTransactionFile(num)
int	num;
{
	char	filename[50];
	int	i;

	sprintf (filename, "%s-%d", filebase, num);
	unlink (filename);

	for (i = 0; i < NUM_CACHED_FILES; i++)
		if (cache[i] == num)
			cache[i] = -1;
}

/*
**  If this file is already in the cache, do nothing.  Otherwise, run the 
**  edsc command to fetch it, and note it as cached.
*/

GetTransactionFile(num)
int	num;
{
	char	filename[50], command[LONGNAMELEN + 25];
	int	i;
	char	*retval;

	if (num == 0)
		return(-1);

	for (i = 0; i < NUM_CACHED_FILES; i++)
		if (cache[i] == num) {
			return (0);
		}

	sprintf (filename, "%s-%d", filebase, num);

/*
** If file doesn't exist, go get it
*/
	sprintf (command, "(gtf %s %d %s)\n", filename, num, currentmtglong);
	retval = RunCommand (command, NULL, NULL, True);

	if ((int) retval <= 0) 
		return(-1);

	myfree (retval);

	for (i = 0; i < NUM_CACHED_FILES; i++)
		if (cache[i] == -1) {
			cache[i] = num;
			break;
		}

	return (num);
}

/*
** Return the highestseen from mtg.  Also set first, last, and highestseen.
** Set current to highestseen if it's not already set (ie, we've just
** entered the meeting)
**
** Return -1 if something went wrong.
*/

int
SetUpTransactionNumbers()
{

	char	command[LONGNAMELEN + 25];
	char	*retval;
	static int	oldhighestseen = 0;

	sprintf (command, "(gmi %s)\n", currentmtglong);

	retval = RunCommand (command, NULL, NULL, True);
	if ((int) retval <= 0) return (-1);

	sscanf (retval, "(\"%*[^\"]\" \"%*[^\"]\" \"%*[^\"]\" %d %d %*d %*d \"%*[^\"]\" \"%*[^\"]\" %*d \"%[^\"]\" %d",
		&first, &last, axis, &highestseen);

	oldhighestseen = highestseen;
			
	if (last == 0 && first == 0) {
		fprintf (stderr,"xdsc:  Reply out of sync with request!\n");
		fprintf (stderr,"xdsc:  requested '%s', got '%s'\n",command, retval);
		PutUpWarning(	"INTERNAL ERROR DETECTED", 
				"I suggest you quit immediately to avoid\npossible corruption of your .meetings file.", True);
		myfree(retval);
		return (-1);
	}
	myfree(retval);

	if (highestseen > last) {
		PutUpWarning(	"WARNING",
				"The highest-read transaction in this\nmeeting no longer exists.\nGoing to end of the meeting.", True);
		highestseen = last;
	}

	if (debug) {
		fprintf (stderr, "highestseen message of %s is %d\n",
				currentmtglong, highestseen);

		fprintf (stderr, "first is %d, last is %d\n",first, last);
	}

	GoToTransaction (highestseen, False);

	return (highestseen);
}

MarkLastRead()
{
	static char	command[LONGNAMELEN + 25];

	if (*currentmtglong)  {
		sprintf (	command, 
				"(ss %d %s)\n", 
				highestseen,
				currentmtglong);
		if (edscversion >= 24)
			(void) RunCommand (command, NULL, NULL, True);
		else
			(void) RunCommand (command, NULL, NULL, False);
	}

}

TransactionNum(arg)
int	arg;
{
	switch (arg) {
		case NEXT:
			return(next);
		case PREV:
			return(prev);
		case NREF:
			return(nref);
		case PREF:
			return(pref);
		case FIRST:
			return(first);
		case LAST:
			return(last);
		case CURRENT:
			return(current);
		case HIGHESTSEEN:
			return(highestseen);
		default:
			fprintf (stderr, "Unknown arg to TransactionNum: %d\n", arg);
			return(0);
	}
}

MoveToMeeting(which)
int	which;
{
	Arg	args[2];
	Boolean	transactionflag = False;
/*
** If we're currently showing transactions, we have to restore the top-level
** list of meetings.
*/
	if (topscreen == LISTTRNS) {
		XawTextDisableRedisplay(toptextW);
		TopSelect (NULL, 4 + (1 << 4));
		transactionflag = True;
	}

/*
** HighlightNewItem will eventually call SaveMeetingNames and 
** SetUpTransactionNumbers to get the next, prev, etc. vars set.
*/
	if (HighlightNewItem(toptextW, which, True) == 0) {

/*
** Need to restore transaction list?
*/
		if (transactionflag == True) {
			TopSelect (NULL, 4 + (0 << 4));
			XawTextEnableRedisplay(toptextW);
		}

/*
** Clear out bottom text window.
*/
		XtSetArg (args[0], XtNstring, "");
		XtSetArg (args[1], XtNlength, 0);
		XtSetValues (bottextW, args, 1);

/*
** If we're at the end of the meeting, make it so the "next" button
** will show us the last transaction.
*/
		if (next == 0)
			next = last;

		CheckButtonSensitivity(BUTTONS_ON);
		return (0);
	}

	else {
		return (-1);
	}
}

/*
** Make sure bottom buttons are set appropriately, according to context
** and mode.
**
** Mode = BUTTONS_UPDATE: turn on appropriate buttons, if sensitive = True.
** Mode = BUTTONS_OFF:  Turn off all buttons.  Set sensitive = False.
** Mode = BUTTONS_ON:  Turn on appropriate buttons.  Set sensitive = True.
**
*/
CheckButtonSensitivity(mode)
int mode;
{
	static Boolean	sensitive = True;
	Arg		args[2];

	if (mode == BUTTONS_ON) sensitive = True;
	if (mode == BUTTONS_OFF) sensitive = False;

	if (next && sensitive) {
		XtSetArg(args[0], XtNsensitive, True);
		XtSetArg(args[1], XtNborderWidth, 1);
	}
	else {
		XtSetArg(args[0], XtNsensitive, False);
		XtSetArg(args[1], XtNborderWidth, 4);
	}
	XtSetValues ((toplevelbuttons[1][0]).button, args, 2);

	if (prev && sensitive)
		XtSetArg(args[0], XtNsensitive, True);
	else
		XtSetArg(args[0], XtNsensitive, False);
	XtSetValues ((toplevelbuttons[1][1]).button, args, 1);

	if (nref && sensitive)
		XtSetArg(args[0], XtNsensitive, True);
	else
		XtSetArg(args[0], XtNsensitive, False);
	XtSetValues ((toplevelbuttons[1][2]).button, args, 1);

	if (pref && sensitive)
		XtSetArg(args[0], XtNsensitive, True);
	else
		XtSetArg(args[0], XtNsensitive, False);
	XtSetValues ((toplevelbuttons[1][3]).button, args, 1);

	if (topscreen == LISTTRNS && sensitive)
		XtSetArg(args[0], XtNsensitive, True);
	else
		XtSetArg(args[0], XtNsensitive, False);
	XtSetValues ((toplevelbuttons[0][5]).button, args, 1);

	if (axis[0] == ' ' && axis[6] == ' ')
		XtSetArg(args[0], XtNsensitive, False);
	else
		XtSetArg(args[0], XtNsensitive, True);
	XtSetValues ((toplevelbuttons[1][5]).button, args, 1);

	if (axis[0] == ' ')
		XtSetArg(args[0], XtNsensitive, False);
	else
		XtSetArg(args[0], XtNsensitive, True);
	XtSetValues ((toplevelbuttons[1][5]).nextrec->button, args, 1);

	if (axis[6] == ' ')
		XtSetArg(args[0], XtNsensitive, False);
	else
		XtSetArg(args[0], XtNsensitive, True);
	XtSetValues ((toplevelbuttons[1][5]).nextrec->nextrec->button, args, 1);

	XtSetArg(args[0], XtNsensitive, False);
	XtSetValues ((toplevelbuttons[1][6]).nextrec->nextrec->button, args, 1);
}
