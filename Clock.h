//--------------------------------------------------------------------------------------------------
// observatory site and time
#pragma once

volatile unsigned long centisecondLAST;

class Clock {
  public:
    // sets date/time from NV and/or the various TLS sources
    // and sets up an event to tick the centisecond sidereal clock
    void init(LI site);

    // set and apply the site longitude, necessary for LAST calculations
    void setSite(LI site);

    // handle date/time commands
    bool command(char reply[], char command[], char parameter[], bool *supressFrame, bool *numericReply, CommandErrors *commandError);

    // gets local apparent sidereal time in hours
    double getLAST();

    // callback to tick the centisecond sidereal clock
    void tick();

  private:

    // sets the time in hours that have passed in this Julian Day
    void setTime(JulianDate julianDate);

    // gets the time in hours that have passed in this Julian Day
    double getTime();

    // sets the time in sidereal hours
    void setSiderealTime(JulianDate julianDate, double time);

    // gets the time in sidereal hours
    double getSiderealTime();

    // adjust time (hours) into the 0 to 24 range
    double backInHours(double time);

    // adjust time (hours) into the -12 to 12 range
    double backInHourAngle(double time);

    // convert julian date/time to local apparent sidereal time
    double julianDateToLAST(JulianDate julianDate);

    // convert julian date/time to greenwich apparent sidereal time
    double julianDateToGAST(JulianDate julianDate);

    // convert Gregorian date (year, month, day) to Julian Day
    JulianDate gregorianToJulianDay(GregorianDate date);

    // convert Julian Day to Gregorian date (year, month, day)
    GregorianDate julianDayToGregorian(JulianDate julianDate);

    LI site;
    JulianDate ut1;
    double centisecondHOUR = 0;
    unsigned long centisecondSTART = 0;

    bool dateIsReady = false;
    bool timeIsReady = false;
};

// instantiate and callback wrappers
Clock clock;
void clockTickWrapper() { clock.tick(); }

void Clock::init(LI site) {
  setSite(site);

  // period ms (0=idle), duration ms (0=forever), repeat, priority (highest 0..7 lowest), task_handle
  uint8_t handle = tasks.add(0, 0, true, 0, clockTickWrapper, "ClkTick");
  tasks.requestHardwareTimer(handle, 3, 1);
  tasks.setPeriodSubMicros(handle, lround(160000.0/SIDEREAL_RATIO));

  GregorianDate date;
  date.year = 2021; date.month  = 2; date.day = 7;
  date.hour = 12;   date.minute = 0; date.second = 0; date.centisecond = 0;
  date.timezone = 5;

  ut1 = gregorianToJulianDay(date);
  ut1.hour = (date.hour + (date.minute + (date.second + date.centisecond/100.0)/60.0)/60.0) + date.timezone;
  setTime(ut1);
}

void Clock::setSite(LI site) {
  this->site = site;

  // same date and time, just calculates the sidereal time again
  ut1.hour = getTime();
  setSiderealTime(ut1, julianDateToLAST(ut1));
}
    
bool Clock::command(char reply[], char command[], char parameter[], bool *supressFrame, bool *numericReply, CommandErrors *commandError) {
  tasks_mutex_enter(MX_CLOCK_CMD);
  PrecisionMode precisionMode = convert.precision;

  // :Ga#       Get standard time in 12 hour format
  //            Returns: HH:MM:SS#
  if (cmd("Ga"))  {
    double time = backInHours(getTime() - ut1.timezone);
    if (time > 12.0) time -= 12.0;
    convert.doubleToHms(reply, time, PM_HIGH);
    *numericReply = false;
  } else

  // :GC#       Get standard calendar date
  //            Returns: MM/DD/YY#
  if (cmd("GC")) {
    JulianDate julianDay = ut1;
    double hour = getTime() - ut1.timezone;
    while (hour >= 24.0) { hour -= 24.0; julianDay.day += 1.0; }
    if    (hour < 0.0)   { hour += 24.0; julianDay.day -= 1.0; }
    GregorianDate date = julianDayToGregorian(julianDay);
    date.year -= 2000; if (date.year >= 100) date.year -= 100;
    sprintf(reply,"%02d/%02d/%02d", (int)date.month, (int)date.day, (int)date.year);
    *numericReply = false;
  } else

  // :Gc#       Get the Local Standard Time format
  //            Returns: 24#
  if (cmd("Gc")) {
    strcpy(reply, "24");
    *numericReply = false;
  } else

  // :GG#       Get UTC offset time, hours and minutes to add to local time to convert to UTC
  //            Returns: [s]HH:MM#
  if (cmd("GG"))  {
    convert.doubleToHms(reply, ut1.timezone, PM_LOWEST);
    *numericReply=false;
  } else

  // :GL#       Get Local Standard Time in 24 hour format
  //            Returns: HH:MM:SS#
  // :GLH#      Returns: HH:MM:SS.SSSS# (high precision)
  if (cmdH("GL")) {
    if (parameter[0] == 'H') precisionMode = PM_HIGHEST;
    convert.doubleToHms(reply, backInHours(getTime() - ut1.timezone), precisionMode);
    *numericReply = false;
  } else

  // :GS#       Get the Sidereal Time as sexagesimal value in 24 hour format
  //            Returns: HH:MM:SS#
  // :GSH#      Returns: HH:MM:SS.ss# (high precision)
  if (cmdH("GS")) {
    if (parameter[0] == 'H') precisionMode = PM_HIGHEST;
    convert.doubleToHms(reply, getSiderealTime(), precisionMode);
    *numericReply = false;
  } else

  // :GX80#     Get the UT1 Time as sexagesimal value in 24 hour format
  //            Returns: HH:MM:SS.ss#
  if (cmd2("GX80")) {
    convert.doubleToHms(reply, backInHours(getTime()), PM_HIGH);
    *numericReply = false;
  } else

  // :GX81#     Get the UT1 Date
  //            Returns: MM/DD/YY#
  if (cmd2("GX81")) {
    JulianDate julianDay = ut1;
    double hour = getTime();
    while (hour >= 24.0) { hour -= 24.0; julianDay.day += 1.0; }
    if    (hour < 0.0)   { hour += 24.0; julianDay.day -= 1.0; }
    GregorianDate date = julianDayToGregorian(julianDay);
    date.year -= 2000; if (date.year >= 100) date.year -= 100;
    sprintf(reply,"%02d/%02d/%02d", (int)date.month, (int)date.day, (int)date.year);
    *numericReply = false;
  } else

  // :GX89#     Date/time ready status
  //            Return: 0 ready, 1 not ready
  if (cmd2("GX89")) {
    if (dateIsReady && timeIsReady) *commandError = CE_0;
  } else

  // :SC[MM/DD/YY]#
  //            Change standard date to MM/DD/YY
  //            Return: 0 on failure, 1 on success
  if (cmdp("SC"))  {
    GregorianDate date = convert.strToDate(parameter);
    if (date.valid) {
      ut1 = gregorianToJulianDay(date);
      ut1.hour = backInHours(getTime());
      double hour = ut1.hour - ut1.timezone;
      if (hour >= 24.0) { hour -= 24.0; ut1.day += 1.0; } else
      if (hour <  0.0)  { hour += 24.0; ut1.day -= 1.0; }
      setSiderealTime(ut1, julianDateToLAST(ut1));
      // nv.writeFloat(EE_JD, JD);
      dateIsReady = true;
      if (generalError == ERR_SITE_INIT && dateIsReady && timeIsReady) generalError = ERR_NONE;
    } else *commandError = CE_PARAM_FORM;
  } else

  //  :SG[sHH]# or :SG[sHH:MM]# (where MM is 00, 30, or 45)
  //            Set the number of hours added to local time to yield UTC
  //            Return: 0 failure, 1 success
  if (cmdp("SG")) {
    double hour;
    if (convert.tzstrToDouble(&hour, parameter)) {
      if (hour >= -13.75 || hour <= 12.0) {
        ut1.timezone = hour;
        //nv.update(EE_sites+currentSite*25 + 8, b);
      } else *commandError = CE_PARAM_RANGE;
    } else *commandError = CE_PARAM_FORM;
  } else

  //  :SL[HH:MM:SS]# or :SL[HH:MM:SS.SSS]#
  //            Set the local Time
  //            Return: 0 failure, 1 success
  if (cmdp("SL"))  {
    double hour;
    if (convert.hmsToDouble(&hour, parameter, PM_HIGH) || convert.hmsToDouble(&hour, parameter, PM_HIGHEST)) {
      #ifndef ESP32
        // nv.writeFloat(EE_LMT,LMT);
      #endif
      ut1.hour = hour + ut1.timezone;
      setTime(ut1);
      timeIsReady = true;
      if (generalError == ERR_SITE_INIT && dateIsReady && timeIsReady) generalError = ERR_NONE;
    } else *commandError = CE_PARAM_FORM;
  } else

  { tasks_mutex_exit(MX_CLOCK_CMD); return false; }
  tasks_mutex_exit(MX_CLOCK_CMD); return true;
}

void Clock::tick() {
  centisecondLAST++;
}

double Clock::getTime() {
  unsigned long cs;
  noInterrupts();
  cs = centisecondLAST;
  interrupts();
  return centisecondHOUR + csToHours((cs - centisecondSTART)/SIDEREAL_RATIO);
}

void Clock::setTime(JulianDate julianDate) {
  setSiderealTime(julianDate, julianDateToLAST(julianDate));
}

double Clock::getSiderealTime() {
  long cs;
  noInterrupts();
  cs = centisecondLAST;
  interrupts();
  return backInHours(csToHours(cs));
}

void Clock::setSiderealTime(JulianDate julianDate, double time) {
  long cs = lround(hoursToCs(time));
  centisecondHOUR = julianDate.hour;
  centisecondSTART = cs;
  noInterrupts();
  centisecondLAST = cs;
  interrupts();
}

double Clock::backInHours(double time) {
  while (time >= 24.0) time -= 24.0;
  while (time < 0.0)   time += 24.0;
  return time;
}

double Clock::backInHourAngle(double time) {
  while (time >= 12.0) time -= 24.0;
  while (time < -12.0) time += 24.0;
  return time;
}

double Clock::julianDateToLAST(JulianDate julianDate) {
  double gast = julianDateToGAST(julianDate);
  return backInHours(gast - radToHrs(site.longitude));
}

double Clock::julianDateToGAST(JulianDate julianDate) {
  GregorianDate date;

  date = julianDayToGregorian(julianDate);
  date.hour = 0; date.minute = 0; date.second = 0; date.centisecond = 0;
  JulianDate julianDay0 = gregorianToJulianDay(date);
  double D= (julianDate.day - 2451545.0) + julianDate.hour/24.0;
  double D0=(julianDay0.day - 2451545.0);
  double H = julianDate.hour;
  double T = D/36525.0;
  double gmst = 6.697374558 + 0.06570982441908*D0;
  gmst = gmst + SIDEREAL_RATIO*H + 0.000026*T*T;

  // equation of the equinoxes
  double O = 125.04  - 0.052954 *D;
  double L = 280.47  + 0.98565  *D;
  double E = 23.4393 - 0.0000004*D;
  double W = -0.000319*sin(degToRad(O)) - 0.000024*sin(degToRad(2*L));
  double eqeq = W*cos(degToRad(E));
  double gast = gmst + eqeq;

  return backInHours(gast);
}

JulianDate Clock::gregorianToJulianDay(GregorianDate date) {
  JulianDate julianDay;
  
  int y = date.year;
  int m = date.month;
  if (m == 1 || m == 2) { y--; m += 12; }
  double B = 2.0 - floor(y/100.0) + floor(y/400.0);
  julianDay.day = B + floor(365.25*y) + floor(30.6001*(m + 1.0)) + date.day + 1720994.5;
  julianDay.hour = 0;
  julianDay.timezone = date.timezone;

  return julianDay;
}

GregorianDate Clock::julianDayToGregorian(JulianDate julianDate) {
  double A, B, C, D, D1, E, F, G, I;
  GregorianDate date;
  
  I = floor(julianDate.day + 0.5);
 
  F = 0.0;
  if (I > 2299160.0) {
    A = int((I - 1867216.25)/36524.25);
    B = I + 1.0 + A - floor(A/4.0);
  } else B = I;

  C = B + 1524.0;
  D = floor((C - 122.1)/365.25);
  E = floor(365.25*D);
  G = floor((C - E)/30.6001);

  D1 = C - E + F - floor(30.6001*G);
  date.day = floor(D1);
  if (G < 13.5)         date.month = floor(G - 1.0);    else date.month = floor(G - 13.0);
  if (date.month > 2.5) date.year  = floor(D - 4716.0); else date.year  = floor(D - 4715.0);
  date.timezone = julianDate.timezone;

  return date;
}