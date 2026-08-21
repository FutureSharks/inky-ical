// Copy this file to secrets.h and fill in your values
// secrets.h is in .gitignore - it won't be committed to GitHub

#ifndef SECRETS_H
#define SECRETS_H

// ===== WiFi Configuration =====
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ===== Google Calendar Configuration =====
// Get from: Google Calendar → Settings → Calendar name → Integrate calendar
// Copy "Public address in iCalendar format" (ends with .ics)
// IMPORTANT: Replace @ with %40 in the URL
// Add as many URLs as you like - events from all of them are merged and
// shown in chronological order.
#define CALENDAR_URLS { \
  "https://calendar.google.com/calendar/ical/YOUR_CALENDAR_ID%40gmail.com/public/basic.ics", \
}

#endif
