#include "oscUtility.h"
#include <sys/wait.h>
#include <unistd.h>

perimeterFilter perimeterFilters[MAX_FILTERS];
int filterCount = 0;
int messagePrintingEnabled = 0;

typedef struct {
    const char* pattern;
    const char* action;
    const char* description;
} DefaultFilter;

static const DefaultFilter defaultFilters[] = {
    {"/avatar/parameters/MediaPlay", "@media-play", "Media play/pause control"},
    {"/avatar/parameters/MediaStop", "@media-stop", "Media stop control"},
    {"/avatar/parameters/MediaNext", "@media-next", "Media next track"},
    {"/avatar/parameters/MediaPrev", "@media-prev", "Media previous track"},
    
    {"/avatar/parameters/MuteSelf", "@media-play", "Toggle self mute (mapped to play/pause)"},
    {"/avatar/parameters/Voice", "@media-play", "Voice activation (mapped to play/pause)"},
    
    {"/avatar/parameters/GestureLeft", "@media-prev", "Left gesture (previous track)"},
    {"/avatar/parameters/GestureRight", "@media-next", "Right gesture (next track)"},
    
    {NULL, NULL, NULL}
};

int canExecuteAction(perimeterFilter* filter) {
    time_t currentTime = time(NULL);
    
    int countDiff = filter->count - filter->lastExecutionCount;
    int countOK = (countDiff >= 2);
    
    int timeOK = 0;
    double timeDiff = 0.0;
    
    if (filter->lastExecutionTime == 0) {
        timeOK = 1; // First execution
        timeDiff = 0.0;
    } else {
        timeDiff = difftime(currentTime, filter->lastExecutionTime);
        timeOK = (timeDiff >= RATE_LIMIT_SECONDS);
    }
    
    if (messagePrintingEnabled) {
        printf("Rate limit check: Count diff=%d (need >=2, %s), Time diff=%.1fs (need >=%ds, %s)\n",
               countDiff, countOK ? "OK" : "BLOCKED",
               timeDiff, RATE_LIMIT_SECONDS, timeOK ? "OK" : "BLOCKED");
    }
    
    // Both conditions must be met
    if (countOK && timeOK) {
        filter->lastExecutionTime = currentTime;
        return 1;
    }
    
    return 0;
}

void addPerimeterFilter(const char* pattern) {
    if (filterCount >= MAX_FILTERS) {
        printf("Maximum number of filters reached (%d)\n", MAX_FILTERS);
        return;
    }

    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            printf("Filter '%s' already exists\n", pattern);
            return;
        }
    }

    strcpy(perimeterFilters[filterCount].pattern, pattern);
    perimeterFilters[filterCount].count = 0;
    perimeterFilters[filterCount].enabled = 1;
    perimeterFilters[filterCount].lastReceived = 0;
    perimeterFilters[filterCount].triggerAction = 0;
    perimeterFilters[filterCount].lastExecutionCount = 0;
    perimeterFilters[filterCount].lastExecutionTime = 0;
    memset(perimeterFilters[filterCount].action, 0, MAX_ACTION_LENGTH);
    filterCount++;
    
    printf("Added filter: '%s'\n", pattern);
    saveConfig();
}

void removePerimeterFilter(const char* pattern) {
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            for (int j = i; j < filterCount - 1; j++) {
                perimeterFilters[j] = perimeterFilters[j + 1];
            }
            filterCount--;
            printf("Removed filter: '%s'\n", pattern);
            saveConfig();
            return;
        }
    }
    printf("Filter '%s' not found\n", pattern);
}

void listParameterFilters(void) {
    if (filterCount == 0) {
        printf("No parameter filters configured\n");
        return;
    }

    printf("Parameter Filters (Rate Limited: >=2 counts AND >=%ds):\n", RATE_LIMIT_SECONDS);
    printf("%-40s %-8s %-8s %-8s %-8s %-15s %-15s %s\n", "Pattern", "Count", "Status", "Action", "LastExec", "Last Received", "Last Executed", "Command");
    printf("%-40s %-8s %-8s %-8s %-8s %-15s %-15s %s\n", "-------", "-----", "------", "------", "--------", "-------------", "-------------", "-------");
    
    for (int i = 0; i < filterCount; i++) {
        char timeStr[64] = "Never";
        char execTimeStr[64] = "Never";
        
        if (perimeterFilters[i].lastReceived > 0) {
            struct tm *tm_info = localtime(&perimeterFilters[i].lastReceived);
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", tm_info);
        }
        
        if (perimeterFilters[i].lastExecutionTime > 0) {
            struct tm *tm_info = localtime(&perimeterFilters[i].lastExecutionTime);
            strftime(execTimeStr, sizeof(execTimeStr), "%H:%M:%S", tm_info);
        }
        
        printf("%-40s %-8d %-8s %-8s %-8d %-15s %-15s %s\n",
               perimeterFilters[i].pattern,
               perimeterFilters[i].count,
               perimeterFilters[i].enabled ? "ON" : "OFF",
               perimeterFilters[i].triggerAction ? "ON" : "OFF",
               perimeterFilters[i].lastExecutionCount,
               timeStr,
               execTimeStr,
               perimeterFilters[i].action[0] ? perimeterFilters[i].action : "None");
    }
}

void clearParameterFilters(void) {
    filterCount = 0;
    printf("All parameter filters cleared\n");
    saveConfig();
}

void resetFilterCounts(void) {
    for (int i = 0; i < filterCount; i++) {
        perimeterFilters[i].count = 0;
        perimeterFilters[i].lastReceived = 0;
        perimeterFilters[i].lastExecutionCount = 0;
        perimeterFilters[i].lastExecutionTime = 0;
    }
    printf("All filter counts and rate limits reset\n");
    saveConfig();
}

void enableFilter(const char* pattern) {
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            perimeterFilters[i].enabled = 1;
            printf("Filter '%s' enabled\n", pattern);
            saveConfig();
            return;
        }
    }
    printf("Filter '%s' not found\n", pattern);
}

void disableFilter(const char* pattern) {
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            perimeterFilters[i].enabled = 0;
            printf("Filter '%s' disabled\n", pattern);
            saveConfig();
            return;
        }
    }
    printf("Filter '%s' not found\n", pattern);
}

void toggleMessagePrinting(void) {
    messagePrintingEnabled = !messagePrintingEnabled;
    printf("Message printing %s\n", messagePrintingEnabled ? "ENABLED" : "DISABLED");
}

void enableMessagePrinting(void) {
    messagePrintingEnabled = 1;
    printf("Message printing ENABLED\n");
}

void disableMessagePrinting(void) {
    messagePrintingEnabled = 0;
    printf("Message printing DISABLED\n");
}

int isMessagePrintingEnabled(void) {
    return messagePrintingEnabled;
}

int checkParameterFilter(const char* parameter) {
    int matched = 0;
    time_t currentTime = time(NULL);
    
    for (int i = 0; i < filterCount; i++) {
        if (perimeterFilters[i].enabled && strstr(parameter, perimeterFilters[i].pattern) != NULL) {
            perimeterFilters[i].count++;
            perimeterFilters[i].lastReceived = currentTime;
            
            if (messagePrintingEnabled) {
                printf("FILTER MATCH: '%s' (Count: %d, LastExec: %d)\n",
                       perimeterFilters[i].pattern, perimeterFilters[i].count, 
                       perimeterFilters[i].lastExecutionCount);
            }
            
            if (perimeterFilters[i].triggerAction && perimeterFilters[i].action[0]) {
                if (canExecuteAction(&perimeterFilters[i])) {
                    if (messagePrintingEnabled) {
                        printf("Executing action (BOTH rate limits OK): %s\n", 
                               perimeterFilters[i].action);
                    }
                    
                    executeAction(perimeterFilters[i].action);
                    perimeterFilters[i].lastExecutionCount = perimeterFilters[i].count;
                } else {
                    if (messagePrintingEnabled) {
                        int countDiff = perimeterFilters[i].count - perimeterFilters[i].lastExecutionCount;
                        double timeDiff = perimeterFilters[i].lastExecutionTime > 0 ? 
                            difftime(currentTime, perimeterFilters[i].lastExecutionTime) : 0.0;
                        
                        printf("Action RATE LIMITED - Count: %d (need >=2), Time: %.1fs (need >=%ds): %s\n", 
                               countDiff, timeDiff, RATE_LIMIT_SECONDS, perimeterFilters[i].action);
                    }
                }
            }
            
            matched = 1;
        }
    }
    return matched;
}

void setFilterAction(const char* pattern, const char* action) {
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            strncpy(perimeterFilters[i].action, action, MAX_ACTION_LENGTH - 1);
            perimeterFilters[i].action[MAX_ACTION_LENGTH - 1] = '\0';
            perimeterFilters[i].triggerAction = 1;
            printf("Set action for filter '%s': %s (Rate limited: >=2 counts AND >=%ds)\n", 
                   pattern, action, RATE_LIMIT_SECONDS);
            saveConfig();
            return;
        }
    }
    printf("Filter '%s' not found\n", pattern);
}

void toggleFilterAction(const char* pattern) {
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            perimeterFilters[i].triggerAction = !perimeterFilters[i].triggerAction;
            printf("Filter '%s' action %s\n", pattern, 
                   perimeterFilters[i].triggerAction ? "enabled" : "disabled");
            saveConfig();
            return;
        }
    }
    printf("Filter '%s' not found\n", pattern);
}

void executeAction(const char* action) {
    if (action[0] == '@') {
        char actionName[256];
        char parameter[256] = {0};
        
        if (sscanf(action, "@%255[^:]:%255s", actionName, parameter) >= 1) {
            if (executeBuiltinAction(actionName, parameter[0] ? parameter : NULL)) {
                return; 
            }
        }
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", action, (char *)NULL);
        exit(1);
    } else if (pid > 0) {
    } else {
        perror("fork failed");
    }
}

void setupDefaultFilters(void) {
    printf("Setting up default media control filters (Rate limited: >=2 counts AND >=%ds)...\n", RATE_LIMIT_SECONDS);
    
    for (int i = 0; defaultFilters[i].pattern != NULL; i++) {
        int exists = 0;
        for (int j = 0; j < filterCount; j++) {
            if (strcmp(perimeterFilters[j].pattern, defaultFilters[i].pattern) == 0) {
                exists = 1;
                break;
            }
        }
        
        if (!exists && filterCount < MAX_FILTERS) {
            strcpy(perimeterFilters[filterCount].pattern, defaultFilters[i].pattern);
            strcpy(perimeterFilters[filterCount].action, defaultFilters[i].action);
            perimeterFilters[filterCount].count = 0;
            perimeterFilters[filterCount].enabled = 1;
            perimeterFilters[filterCount].lastReceived = 0;
            perimeterFilters[filterCount].triggerAction = 1; 
            perimeterFilters[filterCount].lastExecutionCount = 0;
            perimeterFilters[filterCount].lastExecutionTime = 0;
            filterCount++;
            
            printf("Added default filter: %s -> %s\n", 
                   defaultFilters[i].pattern, defaultFilters[i].description);
        }
    }
    
    saveConfig();
    printf("Default filters setup complete! All actions require BOTH >=2 counts AND >=%d second(s).\n", RATE_LIMIT_SECONDS);
}

void listDefaultFilters(void) {
    printf("Available Default Filters (Rate Limited: >=2 counts AND >=%ds):\n", RATE_LIMIT_SECONDS);
    printf("%-40s %-20s %s\n", "Pattern", "Action", "Description");
    printf("%-40s %-20s %s\n", "-------", "------", "-----------");
    
    for (int i = 0; defaultFilters[i].pattern != NULL; i++) {
        printf("%-40s %-20s %s\n", 
               defaultFilters[i].pattern, 
               defaultFilters[i].action, 
               defaultFilters[i].description);
    }
    
    printf("\nUse 'defaults' command to add all default filters.\n");
    printf("All actions require BOTH >=2 message counts AND >=%d second(s) to execute.\n", RATE_LIMIT_SECONDS);
}

void addDefaultFilter(const char* pattern, const char* action, const char* description) {
    (void)description; 
    
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            printf("Filter '%s' already exists\n", pattern);
            return;
        }
    }
    
    if (filterCount >= MAX_FILTERS) {
        printf("Maximum number of filters reached\n");
        return;
    }
    
    strcpy(perimeterFilters[filterCount].pattern, pattern);
    strcpy(perimeterFilters[filterCount].action, action);
    perimeterFilters[filterCount].count = 0;
    perimeterFilters[filterCount].enabled = 1;
    perimeterFilters[filterCount].lastReceived = 0;
    perimeterFilters[filterCount].triggerAction = 1;
    perimeterFilters[filterCount].lastExecutionCount = 0;
    perimeterFilters[filterCount].lastExecutionTime = 0;
    filterCount++;
    
    printf("Added default filter: '%s' with action '%s' (Rate limited: >=2 counts AND >=%ds)\n", 
           pattern, action, RATE_LIMIT_SECONDS);
    saveConfig();
}

void listBuiltinActions(void) {
    printf("Built-in Media Control Actions (Rate Limited: >=2 counts AND >=%ds):\n", RATE_LIMIT_SECONDS);
    printf("  @media-play               - Play/pause media\n");
    printf("  @media-stop               - Stop media\n");
    printf("  @media-next               - Next track\n");
    printf("  @media-prev               - Previous track\n");
    
    printf("\nExamples:\n");
    printf("  action /avatar/parameters/MediaPlay @media-play\n");
    printf("  action /avatar/parameters/MediaNext @media-next\n");
    printf("\nNote: All actions require BOTH count and time-based rate limiting.\n");
}

int executeBuiltinAction(const char* actionName, const char* parameter) {
    (void)parameter; 
    
    if (strcmp(actionName, "media-play") == 0) {
        mediaPlayPause();
        return 1;
    } else if (strcmp(actionName, "media-stop") == 0) {
        mediaStop();
        return 1;
    } else if (strcmp(actionName, "media-next") == 0) {
        mediaNext();
        return 1;
    } else if (strcmp(actionName, "media-prev") == 0) {
        mediaPrevious();
        return 1;
    }
    
    printf("Unknown built-in action: @%s\n", actionName);
    return 0;
}

int saveConfig(void) {
    FILE *file = fopen(CONFIG_FILE, "w");
    if (!file) {
        perror("Failed to open config file for writing");
        return -1;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"messagePrintingEnabled\": %s,\n", messagePrintingEnabled ? "true" : "false");
    fprintf(file, "  \"rateLimitSeconds\": %d,\n", RATE_LIMIT_SECONDS);
    fprintf(file, "  \"filters\": [\n");
    
    for (int i = 0; i < filterCount; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"pattern\": \"%s\",\n", perimeterFilters[i].pattern);
        fprintf(file, "      \"enabled\": %s,\n", perimeterFilters[i].enabled ? "true" : "false");
        fprintf(file, "      \"triggerAction\": %s,\n", perimeterFilters[i].triggerAction ? "true" : "false");
        fprintf(file, "      \"action\": \"%s\",\n", perimeterFilters[i].action);
        fprintf(file, "      \"lastExecutionCount\": %d,\n", perimeterFilters[i].lastExecutionCount);
        fprintf(file, "      \"lastExecutionTime\": %ld\n", (long)perimeterFilters[i].lastExecutionTime);
        fprintf(file, "    }%s\n", (i < filterCount - 1) ? "," : "");
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    return 0;
}

int loadConfig(void) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        printf("No config file found, starting with empty filters\n");
        return 0;
    }

    char line[1024];
    char pattern[MAX_PATTERN_LENGTH] = {0};
    char action[MAX_ACTION_LENGTH] = {0};
    int enabled = 1;
    int triggerAction = 0;
    int lastExecutionCount = 0;
    time_t lastExecutionTime = 0;
    int inFilter = 0;
    
    filterCount = 0;
    messagePrintingEnabled = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "\"messagePrintingEnabled\":")) {
            char enabledStr[10];
            sscanf(line, " \"messagePrintingEnabled\": %9s", enabledStr);
            messagePrintingEnabled = (strstr(enabledStr, "true") != NULL);
        } else if (strstr(line, "\"pattern\":")) {
            sscanf(line, " \"pattern\": \"%255[^\"]\"", pattern);
            inFilter = 1;
        } else if (strstr(line, "\"enabled\":")) {
            char enabledStr[10];
            sscanf(line, " \"enabled\": %9s", enabledStr);
            enabled = (strstr(enabledStr, "true") != NULL);
        } else if (strstr(line, "\"triggerAction\":")) {
            char triggerStr[10];
            sscanf(line, " \"triggerAction\": %9s", triggerStr);
            triggerAction = (strstr(triggerStr, "true") != NULL);
        } else if (strstr(line, "\"action\":")) {
            sscanf(line, " \"action\": \"%511[^\"]\"", action);
        } else if (strstr(line, "\"lastExecutionCount\":")) {
            sscanf(line, " \"lastExecutionCount\": %d", &lastExecutionCount);
        } else if (strstr(line, "\"lastExecutionTime\":")) {
            long temp;
            sscanf(line, " \"lastExecutionTime\": %ld", &temp);
            lastExecutionTime = (time_t)temp;
        } else if (strstr(line, "}") && inFilter) {
            if (filterCount < MAX_FILTERS && pattern[0]) {
                strcpy(perimeterFilters[filterCount].pattern, pattern);
                perimeterFilters[filterCount].enabled = enabled;
                perimeterFilters[filterCount].triggerAction = triggerAction;
                strcpy(perimeterFilters[filterCount].action, action);
                perimeterFilters[filterCount].count = 0;
                perimeterFilters[filterCount].lastReceived = 0;
                perimeterFilters[filterCount].lastExecutionCount = lastExecutionCount;
                perimeterFilters[filterCount].lastExecutionTime = lastExecutionTime;
                filterCount++;
            }
            
            memset(pattern, 0, sizeof(pattern));
            memset(action, 0, sizeof(action));
            enabled = 1;
            triggerAction = 0;
            lastExecutionCount = 0;
            lastExecutionTime = 0;
            inFilter = 0;
        }
    }
    
    fclose(file);
    printf("Loaded %d filters from config (Rate limit: >=2 counts AND >=%ds)\n", filterCount, RATE_LIMIT_SECONDS);
    return 0;
}
