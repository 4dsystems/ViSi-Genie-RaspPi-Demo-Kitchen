/*
 * kitchenPi.c:
 *	The ultimate kitchen gadget - maybe.
 *	Part 1 is a clock and 2 timers.
 *
 *	Gordon Henderson, January 2013
 ***********************************************************************
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <math.h>

#include <geniePi.h>

#ifndef	TRUE
#  define TRUE  (1==1)
#  define FALSE (1==2)
#endif

#define	FORM_HOME	0
#define	FORM_TIMERS	4
#define	FORM_TIME_UP	6
#define	FORM_ALARM	7

// Alarm Buttons

#define	ALARM_H_UP	19
#define	ALARM_H_DOWN	21
#define	ALARM_M_UP	20
#define	ALARM_M_DOWN	22

#define	ALARM_H_DIGITS	 3
#define	ALARM_M_DIGITS	 4

#define	ALARM_DAILY	18
#define	ALARM_ALARM	26

#define	ALARM_NUM_LEDS	 2

int alarmHours  = 0 ;
int alarmMins   = 0 ;
int alarmActive = FALSE ;
int alarmDaily  = FALSE ;

static const int alarmLeds [] = { 0, 5 } ;

// The Stopwatch

#define	SWATCH_M_DIGITS		14
#define	SWATCH_S_DIGITS		13

#define	SWATCH_START_STOP	30
#define	SWATCH_ZERO		31

int stopwatchS       = 0 ;
int stopwatchM       = 0 ;
int stopwatchRunning = FALSE ;


// Timers 

#define	TIMER1_H_UP		 7
#define	TIMER1_H_DOWN		 9
#define	TIMER1_M_UP		 8
#define	TIMER1_M_DOWN		10

#define	TIMER1_H1_DIGITS	 5
#define	TIMER1_M1_DIGITS	 6

#define	TIMER1_H2_DIGITS	10
#define	TIMER1_M2_DIGITS	 9
#define	TIMER1_S2_DIGITS	15

#define	TIMER1_START_STOP	 6

#define	TIMER2_H_UP		11
#define	TIMER2_H_DOWN		13
#define	TIMER2_M_UP		12
#define	TIMER2_M_DOWN		14

#define	TIMER2_H1_DIGITS	 7
#define	TIMER2_M1_DIGITS	 8

#define	TIMER2_H2_DIGITS	12
#define	TIMER2_M2_DIGITS	11
#define	TIMER2_S2_DIGITS	16

#define	TIMER2_START_STOP	17

#define	TIMER_NUM_LEDS		 4

int timer1SetHours = 0 ;
int timer1SetMins  = 0 ;
int timer1Hours    = 0 ;
int timer1Mins     = 0 ;
int timer1Secs     = 0 ;
int timer1Running  = FALSE ;

int timer2SetHours = 0 ;
int timer2SetMins  = 0 ;
int timer2Hours    = 0 ;
int timer2Mins     = 0 ;
int timer2Secs     = 0 ;
int timer2Running  = FALSE ;

static const int timer1Leds [] = { 1, 3, 8, 7 } ;
static const int timer2Leds [] = { 2, 4, 6, 9 } ;


/*
 * clockForm:
 *	This is run as a concurrent thread and will update the
 *	clock form on the display continually.
 *********************************************************************************
 */

static void *clockForm (void *data)
{
  int i ;
  time_t tt ;
  struct tm timeData ;

// What's the time, Mr Wolf?

  sleep (1) ;

  for (;;)
  {
    tt = time (NULL) ;
    while (time (NULL) == tt)
      usleep (100000) ;	// 100mS - So it might be 1/10 sec. slow...

    tt = time (NULL) ;
    (void)localtime_r (&tt, &timeData) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, 0, timeData.tm_hour) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, 2, timeData.tm_min) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, 1, timeData.tm_sec) ;

    for (i = 0 ; i < ALARM_NUM_LEDS ; ++i)
      genieWriteObj (GENIE_OBJ_LED, alarmLeds [i], alarmActive ? 1 : 0) ;
  }

 return (void *)NULL ;
}


/*
 * updateAlarm:
 *	Update our alarm based on button pushes on the display
 *********************************************************************************
 */

void updateAlarm (int button)
{
  switch (button)
  {
    case ALARM_H_UP:
      if (++alarmHours == 24) alarmHours = 0 ;
      break ;

    case ALARM_H_DOWN:
      if (--alarmHours == -1) alarmHours = 23 ;
      break ;

    case ALARM_M_UP:
      if (++alarmMins == 60) alarmMins = 0 ;
      break ;

    case ALARM_M_DOWN:
      if (--alarmMins == -1) alarmMins = 59 ;
      break ;

    case ALARM_DAILY:
      alarmDaily = !alarmDaily ;
      break ;

    case ALARM_ALARM:
      alarmActive = !alarmActive ;
      break ;
  }

  genieWriteObj (GENIE_OBJ_LED_DIGITS, ALARM_H_DIGITS, alarmHours) ;
  genieWriteObj (GENIE_OBJ_LED_DIGITS, ALARM_M_DIGITS, alarmMins) ;
}


/*
 * updateTimer1:
 *	Update our alarm based on button pushes on the display
 *********************************************************************************
 */

void updateTimer1 (int button)
{
  int i ;

  switch (button)
  {
    case TIMER1_H_UP:
      if (!timer1Running)
	if (++timer1SetHours == 10) timer1SetHours = 0 ;
      break ;

    case TIMER1_H_DOWN:
      if (!timer1Running)
	if (--timer1SetHours == -1) timer1SetHours = 9 ;
      break ;

    case TIMER1_M_UP:
      if (!timer1Running)
	if (++timer1SetMins == 60) timer1SetMins = 0 ;
      break ;

    case TIMER1_M_DOWN:
      if (!timer1Running)
	if (--timer1SetMins == -1) timer1SetMins = 59 ;
      break ;

    case TIMER1_START_STOP:
      if (timer1Running)
      {
	timer1Running = FALSE ;
	for (i = 0 ; i < TIMER_NUM_LEDS ; ++i)
	  genieWriteObj (GENIE_OBJ_LED, timer1Leds [i], 0) ;
      }
      else
      {
	timer1Hours = timer1SetHours ;
	timer1Mins  = timer1SetMins ;
	timer1Secs  = 0 ;
	timer1Running = TRUE ;
	for (i = 0 ; i < TIMER_NUM_LEDS ; ++i)
	  genieWriteObj (GENIE_OBJ_LED, timer1Leds [i], 1) ;
	genieWriteObj (GENIE_OBJ_FORM,  FORM_TIMERS,    0) ;
      }
      break ;
  }

  genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER1_H1_DIGITS, timer1SetHours) ;
  genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER1_M1_DIGITS, timer1SetMins) ;
}


/*
 * updateTimer2:
 *	Update our alarm based on button pushes on the display
 *********************************************************************************
 */

void updateTimer2 (int button)
{
  int i ;

  switch (button)
  {
    case TIMER2_H_UP:
      if (!timer2Running)
	if (++timer2SetHours == 10) timer2SetHours = 0 ;
      break ;

    case TIMER2_H_DOWN:
      if (!timer2Running)
	if (--timer2SetHours == -1) timer2SetHours = 9 ;
      break ;

    case TIMER2_M_UP:
      if (!timer2Running)
	if (++timer2SetMins == 60) timer2SetMins = 0 ;
      break ;

    case TIMER2_M_DOWN:
      if (!timer2Running)
	if (--timer2SetMins == -1) timer2SetMins = 59 ;
      break ;

    case TIMER2_START_STOP:
      if (timer2Running)
      {
	timer2Running = FALSE ;
	for (i = 0 ; i < TIMER_NUM_LEDS ; ++i)
	  genieWriteObj (GENIE_OBJ_LED, timer2Leds [i], 0) ;
      }
      else
      {
	timer2Hours = timer2SetHours ;
	timer2Mins  = timer2SetMins ;
	timer2Secs  = 0 ;
	timer2Running = TRUE ;
	for (i = 0 ; i < TIMER_NUM_LEDS ; ++i)
	  genieWriteObj (GENIE_OBJ_LED, timer2Leds [i], 1) ;
	genieWriteObj (GENIE_OBJ_FORM,  FORM_TIMERS,    0) ;
      }
      break ;
  }

  genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER2_H1_DIGITS, timer2SetHours) ;
  genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER2_M1_DIGITS, timer2SetMins) ;
}


/*
 * timer1:
 *	Thread to update the timer1 on the display
 *********************************************************************************
 */

static void *timer1 (void *data)
{
  int i ;
  time_t tt ;

  for (;;)
  {
//    tt = time (NULL) ;
//    while (time (NULL) == tt)
      usleep (100000) ;

    if (!timer1Running)
      continue ;

    if (--timer1Secs == -1)
    {
      timer1Secs = 59 ;
      if (--timer1Mins == -1)
      {
	timer1Mins = 59 ;
	if (--timer1Hours == -1)	// Time up!
	{
	  timer1Running = FALSE ;
	  timer1Hours = timer1Mins = timer1Secs = 0 ;
	  for (i = 0 ; i < TIMER_NUM_LEDS ; ++i)
	    genieWriteObj (GENIE_OBJ_LED, timer1Leds [i], 0) ;
	  genieWriteObj (GENIE_OBJ_FORM,  FORM_TIME_UP,    0) ;
	  genieWriteObj (GENIE_OBJ_SOUND, 1,             100) ;	// Max. Volume
	  genieWriteObj (GENIE_OBJ_SOUND, 0,               1) ;	// Play Sound 1
	}
      }
    }

    genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER1_H2_DIGITS, timer1Hours) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER1_M2_DIGITS, timer1Mins) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER1_S2_DIGITS, timer1Secs) ;
  }
  return (void *)NULL ;
}


/*
 * timer2:
 *	Thread to update the timer2 on the display
 *********************************************************************************
 */

static void *timer2 (void *data)
{
  int i ;
  time_t tt ;

  for (;;)
  {
//    tt = time (NULL) ;
//    while (time (NULL) == tt)
      usleep (100000) ;

    if (!timer2Running)
      continue ;

    if (--timer2Secs == -1)
    {
      timer2Secs = 59 ;
      if (--timer2Mins == -1)
      {
	timer2Mins = 59 ;
	if (--timer2Hours == -1)	// Time up!
	{
	  timer2Running = FALSE ;
	  timer2Hours = timer2Mins = timer2Secs = 0 ;
	  for (i = 0 ; i < TIMER_NUM_LEDS ; ++i)
	    genieWriteObj (GENIE_OBJ_LED, timer2Leds [i], 0) ;
	  genieWriteObj (GENIE_OBJ_FORM,  FORM_TIME_UP,    0) ;
	  genieWriteObj (GENIE_OBJ_SOUND, 1,             100) ;	// Max. Volume
	  genieWriteObj (GENIE_OBJ_SOUND, 0,               2) ;	// Play Sound 2
	}
      }
    }

    genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER2_H2_DIGITS, timer2Hours) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER2_M2_DIGITS, timer2Mins) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, TIMER2_S2_DIGITS, timer2Secs) ;
  }
  return (void *)NULL ;
}




/*
 * updateStopwatch:
 *	Only 2 buttons to handle here, start/stop and zero
 *********************************************************************************
 */

static void updateStopwatch (int button)
{
  if (button == SWATCH_START_STOP)	// Start or Stop
    stopwatchRunning = !stopwatchRunning ;
  else
  {
    if (stopwatchRunning)		// Don't zero if running
      return ;
    stopwatchM = stopwatchS = 0 ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, SWATCH_M_DIGITS, stopwatchM) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, SWATCH_S_DIGITS, stopwatchS) ;
  }
}


/*
 * stopwatch:
 *	Thread to update the stopwatch on the display
 *********************************************************************************
 */

static void *stopwatch (void *data)
{
  time_t tt ;

  for (;;)
  {
    tt = time (NULL) ;
    while (time (NULL) == tt)
      usleep (100000) ;

    if (!stopwatchRunning)
      continue ;

    if (++stopwatchS == 60)
    {
      stopwatchS = 0 ;
      if (++stopwatchM == 99)
	stopwatchM = 0 ;
    }

    genieWriteObj (GENIE_OBJ_LED_DIGITS, SWATCH_M_DIGITS, stopwatchM) ;
    genieWriteObj (GENIE_OBJ_LED_DIGITS, SWATCH_S_DIGITS, stopwatchS) ;
  }
  return (void *)NULL ;
}


/*
 * handleGenieEvent:
 *	Take a reply off the display and call the appropriate handler for it.
 *********************************************************************************
 */

void handleGenieEvent (struct genieReplyStruct *reply)
{
  if (reply->cmd != GENIE_REPORT_EVENT)
  {
    printf ("Invalid event from the display: 0x%02X\r\n", reply->cmd) ;
    return ;
  }

  /**/ if (reply->object == GENIE_OBJ_WINBUTTON)
  {
    switch (reply->index)
    {
      case ALARM_H_UP: case ALARM_H_DOWN: case ALARM_M_UP: case ALARM_M_DOWN:
      case ALARM_DAILY: case ALARM_ALARM:
	updateAlarm (reply->index) ;
	break ;

      case SWATCH_START_STOP: case SWATCH_ZERO:
	updateStopwatch (reply->index) ;
	break ;

      case TIMER1_H_UP: case TIMER1_H_DOWN: case TIMER1_M_UP: case TIMER1_M_DOWN:
      case TIMER1_START_STOP:
	updateTimer1 (reply->index) ;
	break ;

      case TIMER2_H_UP: case TIMER2_H_DOWN: case TIMER2_M_UP: case TIMER2_M_DOWN:
      case TIMER2_START_STOP:
	updateTimer2 (reply->index) ;
	break ;

      default:
	printf ("Unknown button: %d\n", reply->index) ;
	break ;
    }
  }
  else
    printf ("Unhandled Event: object: %2d, index: %d data: %d [%02X %02X %04X]\r\n",
      reply->object, reply->index, reply->data, reply->object, reply->index, reply->data) ;
}


/*
 *********************************************************************************
 * main:
 *	Run our little demo
 *********************************************************************************
 */

int main ()
{
  pthread_t myThread ;
  struct genieReplyStruct reply ;

  printf ("\n\n\n\n") ;
  printf ("Visi-Genie Kitchen Pi\n") ;
  printf ("=====================\n") ;

// Genie display setup
//	Using the Raspberry Pi's on-board serial port.

  if (genieSetup ("/dev/ttyAMA0", 115200) < 0)
    { fprintf (stderr, "kitchenPi: Can't initialise Genie Display: %s\n", strerror (errno)) ; return 1 ; }

// Setup the display - cold boot:
//	Select form 0 (the clock)
//	Unset the 2 alarm buttons too.

  genieWriteObj (GENIE_OBJ_FORM,      FORM_HOME,   0) ;
  genieWriteObj (GENIE_OBJ_WINBUTTON, ALARM_DAILY, 0) ;
  genieWriteObj (GENIE_OBJ_WINBUTTON, ALARM_ALARM, 0) ;

// Start the threads

  (void)pthread_create (&myThread, NULL, clockForm, NULL) ;
  (void)pthread_create (&myThread, NULL, stopwatch, NULL) ;
  (void)pthread_create (&myThread, NULL, timer1,    NULL) ;
  (void)pthread_create (&myThread, NULL, timer2,    NULL) ;

// Big loop - just wait for events from the display now

  for (;;)
  {
    while (genieReplyAvail ())
    {
      genieGetReply    (&reply) ;
      handleGenieEvent (&reply) ;
    }
    usleep (10000) ; // 10mS - Don't hog the CPU in-case anything else is happening...
  }

  return 0 ;
}
