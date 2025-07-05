#ifndef OSC_UTILITY_H
#define OSC_UTILITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mediaControl.h"
#include "rateLimiter.h"

#define MAX_FILTERS 100
#define MAX_PATTERN_LENGTH 256
#define MAX_ACTION_LENGTH 512
#define CONFIG_FILE "config.json"

typedef struct {
    char pattern[MAX_PATTERN_LENGTH];
    int count;
    int enabled;
    time_t lastReceived;
    char action[MAX_ACTION_LENGTH]; 
    int triggerAction;
    RateLimiter rateLimiter;        
} perimeterFilter;

extern perimeterFilter perimeterFilters[MAX_FILTERS];
extern int filterCount;
extern int messagePrintingEnabled;  

void addPerimeterFilter(const char* pattern);
void removePerimeterFilter(const char* pattern);
void listParameterFilters(void);
void clearParameterFilters(void);
void resetFilterCounts(void);
int checkParameterFilter(const char* perimeter);
void enableFilter(const char* pattern);
void disableFilter(const char* pattern);

void toggleMessagePrinting(void);
void enableMessagePrinting(void);
void disableMessagePrinting(void);
int isMessagePrintingEnabled(void);

int saveConfig(void);
int loadConfig(void);

void setFilterAction(const char* pattern, const char* action);
void toggleFilterAction(const char* pattern);
void executeAction(const char* action);

void setFilterRateLimit(const char* pattern, int count, int seconds);
void listFilterRateLimits(void);
void resetFilterRateLimit(const char* pattern);

void setupDefaultFilters(void);
void listDefaultFilters(void);
void addDefaultFilter(const char* pattern, const char* action, const char* description);

void listBuiltinActions(void);
int executeBuiltinAction(const char* actionName, const char* parameter);

int fileExists(const char* filename);
int generateDefaultConfig(void);

#endif
