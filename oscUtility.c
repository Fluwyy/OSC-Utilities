#include "oscUtility.h"
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include "keyPress.h"

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
    {NULL, NULL, NULL}
};

int fileExists(const char* filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

int generateDefaultConfig(void) {
    printf("Config file '%s' not found. Generating default configuration...\n", CONFIG_FILE);
    
    messagePrintingEnabled = 1;
    filterCount = 0;
    
    for (int i = 0; defaultFilters[i].pattern != NULL && filterCount < MAX_FILTERS; i++) {
        strcpy(perimeterFilters[filterCount].pattern, defaultFilters[i].pattern);
        strcpy(perimeterFilters[filterCount].action, defaultFilters[i].action);
        perimeterFilters[filterCount].count = 0;
        perimeterFilters[filterCount].enabled = 1;
        perimeterFilters[filterCount].lastReceived = 0;
        perimeterFilters[filterCount].triggerAction = 1;
        
        initRateLimiter(&perimeterFilters[filterCount].rateLimiter);
        
        filterCount++;
        
        printf("  -> Added: %s (%s) [Rate: %dc/%ds]\n", 
               defaultFilters[i].pattern, defaultFilters[i].description,
               DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
    }
    
    printf("Saving default config to '%s'...\n", CONFIG_FILE);
    if (saveConfig() == 0) {
        printf("✓ Default config file created successfully!\n");
        printf("✓ Generated %d default media control filters\n", filterCount);
        printf("✓ Message printing enabled\n");
        printf("✓ Default rate limiting: %dc/%ds\n", 
               DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
        return 0;
    } else {
        printf("✗ ERROR: Failed to create config file '%s'\n", CONFIG_FILE);
        return -1;
    }
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
    memset(perimeterFilters[filterCount].action, 0, MAX_ACTION_LENGTH);
    
    initRateLimiter(&perimeterFilters[filterCount].rateLimiter);
    
    filterCount++;
    
    printf("Added filter: '%s' [Default rate limit: %dc/%ds]\n", 
           pattern, DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
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

    printf("Parameter Filters:\n");
    printf("%-40s %-8s %-8s %-8s %-8s %-12s %-15s %-15s %s\n", 
           "Pattern", "Count", "Status", "Action", "LastExec", "Rate Limit", "Last Received", "Last Executed", "Command");
    printf("%-40s %-8s %-8s %-8s %-8s %-12s %-15s %-15s %s\n", 
           "-------", "-----", "------", "------", "--------", "----------", "-------------", "-------------", "-------");
    
    for (int i = 0; i < filterCount; i++) {
        char timeStr[64] = "Never";
        char execTimeStr[64] = "Never";
        char rateLimitStr[32];
        
        if (perimeterFilters[i].lastReceived > 0) {
            struct tm *tm_info = localtime(&perimeterFilters[i].lastReceived);
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", tm_info);
        }
        
        if (perimeterFilters[i].rateLimiter.lastExecutionTime > 0) {
            struct tm *tm_info = localtime(&perimeterFilters[i].rateLimiter.lastExecutionTime);
            strftime(execTimeStr, sizeof(execTimeStr), "%H:%M:%S", tm_info);
        }
        
        formatRateLimitString(&perimeterFilters[i].rateLimiter, rateLimitStr, sizeof(rateLimitStr));
        
        printf("%-40s %-8d %-8s %-8s %-8d %-12s %-15s %-15s %s\n",
               perimeterFilters[i].pattern,
               perimeterFilters[i].count,
               perimeterFilters[i].enabled ? "ON" : "OFF",
               perimeterFilters[i].triggerAction ? "ON" : "OFF",
               perimeterFilters[i].rateLimiter.lastExecutionCount,
               rateLimitStr,
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
        resetRateLimiter(&perimeterFilters[i].rateLimiter);
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

void setFilterRateLimit(const char* pattern, int count, int seconds) {
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            setRateLimitValues(&perimeterFilters[i].rateLimiter, count, seconds);
            printf("Set rate limit for filter '%s': %dc/%ds\n", pattern, count, seconds);
            saveConfig();
            return;
        }
    }
    printf("Filter '%s' not found\n", pattern);
}

void listFilterRateLimits(void) {
    if (filterCount == 0) {
        printf("No parameter filters configured\n");
        return;
    }

    printf("Filter Rate Limits:\n");
    printf("%-40s %-12s %-12s %-10s %s\n", "Pattern", "Min Count", "Min Seconds", "Default?", "Status");
    printf("%-40s %-12s %-12s %-10s %s\n", "-------", "---------", "-----------", "--------", "------");
    
    for (int i = 0; i < filterCount; i++) {
        int count, seconds;
        getRateLimitValues(&perimeterFilters[i].rateLimiter, &count, &seconds);
        
        printf("%-40s %-12d %-12d %-10s %s\n",
               perimeterFilters[i].pattern,
               count,
               seconds,
               isRateLimitDefault(&perimeterFilters[i].rateLimiter) ? "YES" : "NO",
               perimeterFilters[i].enabled ? "ENABLED" : "DISABLED");
    }
}

void resetFilterRateLimit(const char* pattern) {
    for (int i = 0; i < filterCount; i++) {
        if (strcmp(perimeterFilters[i].pattern, pattern) == 0) {
            initRateLimiter(&perimeterFilters[i].rateLimiter);
            printf("Reset rate limit for filter '%s' to defaults: %dc/%ds\n", 
                   pattern, DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
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
                char rateLimitStr[32];
                formatRateLimitString(&perimeterFilters[i].rateLimiter, rateLimitStr, sizeof(rateLimitStr));
                printf("FILTER MATCH: '%s' (Count: %d, LastExec: %d, Rate: %s)\n",
                       perimeterFilters[i].pattern, perimeterFilters[i].count, 
                       perimeterFilters[i].rateLimiter.lastExecutionCount, rateLimitStr);
            }
            
            if (perimeterFilters[i].triggerAction && perimeterFilters[i].action[0]) {
                if (canExecuteWithRateLimit(&perimeterFilters[i].rateLimiter, 
                                          perimeterFilters[i].count, messagePrintingEnabled)) {
                    if (messagePrintingEnabled) {
                        printf("Executing action (rate limits OK): %s\n", 
                               perimeterFilters[i].action);
                    }
                    
                    executeAction(perimeterFilters[i].action);
                    updateRateLimiterExecution(&perimeterFilters[i].rateLimiter, 
                                             perimeterFilters[i].count);
                } else {
                    if (messagePrintingEnabled) {
                        char rateLimitStr[32];
                        formatRateLimitString(&perimeterFilters[i].rateLimiter, rateLimitStr, sizeof(rateLimitStr));
                        printf("Action RATE LIMITED (%s): %s\n", rateLimitStr, perimeterFilters[i].action);
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
            
            char rateLimitStr[32];
            formatRateLimitString(&perimeterFilters[i].rateLimiter, rateLimitStr, sizeof(rateLimitStr));
            printf("Set action for filter '%s': %s [Rate limit: %s]\n", pattern, action, rateLimitStr);
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
            
            if (executeBuiltinKeyAction(actionName, parameter[0] ? parameter : NULL)) {
                return;
            }
        }
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", action, (char *)NULL);
        exit(1);
    } else if (pid > 0) {
        // Non-blocking execution
    } else {
        perror("fork failed");
    }
}

void setupDefaultFilters(void) {
    printf("Setting up default media control filters...\n");
    
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
            
            initRateLimiter(&perimeterFilters[filterCount].rateLimiter);
            
            filterCount++;
            
            printf("Added default filter: %s -> %s [Rate: %dc/%ds]\n", 
                   defaultFilters[i].pattern, defaultFilters[i].description,
                   DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
        }
    }
    
    saveConfig();
    printf("Default filters setup complete!\n");
}

void listDefaultFilters(void) {
    printf("Available Default Filters:\n");
    printf("%-40s %-20s %s\n", "Pattern", "Action", "Description");
    printf("%-40s %-20s %s\n", "-------", "------", "-----------");
    
    for (int i = 0; defaultFilters[i].pattern != NULL; i++) {
        printf("%-40s %-20s %s\n", 
               defaultFilters[i].pattern, 
               defaultFilters[i].action, 
               defaultFilters[i].description);
    }
    
    printf("\nUse 'defaults' command to add all default filters.\n");
    printf("Default rate limiting: %dc/%ds\n", 
           DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
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
    
    initRateLimiter(&perimeterFilters[filterCount].rateLimiter);
    
    filterCount++;
    
    printf("Added default filter: '%s' with action '%s' [Rate: %dc/%ds]\n", 
           pattern, action, DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
    saveConfig();
}

void listBuiltinActions(void) {
    printf("Built-in Media Control Actions:\n");
    printf("  @media-play               - Play/pause media\n");
    printf("  @media-stop               - Stop media\n");
    printf("  @media-next               - Next track\n");
    printf("  @media-prev               - Previous track\n\n");
    
    printf("Built-in KeyPress Actions:\n");
    printf("  @key:<keystring>          - Send custom key press\n");
    printf("  @copy                     - Ctrl+C (copy)\n");
    printf("  @paste                    - Ctrl+V (paste)\n");
    printf("  @cut                      - Ctrl+X (cut)\n");
    printf("  @undo                     - Ctrl+Z (undo)\n");
    printf("  @redo                     - Ctrl+Y (redo)\n");
    printf("  @select-all               - Ctrl+A (select all)\n");
    printf("  @alt-tab                  - Alt+Tab (switch windows)\n");
    printf("  @screenshot               - Print Screen\n\n");
    
    printf("Examples:\n");
    printf("  action /avatar/parameters/MediaPlay @media-play\n");
    printf("  action discordmute @key:ctrl+shift+m\n");
    printf("  action screenshot @screenshot\n");
    printf("  action copy-text @copy\n");
    printf("\nNote: All actions use configurable rate limiting per filter.\n");
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
    
    return 0; 
}

int saveConfig(void) {
    FILE *file = fopen(CONFIG_FILE, "w");
    if (!file) {
        printf("ERROR: Cannot create/write to config file '%s'\n", CONFIG_FILE);
        perror("fopen failed");
        return -1;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"messagePrintingEnabled\": %s,\n", messagePrintingEnabled ? "true" : "false");
    fprintf(file, "  \"defaultRateLimitCount\": %d,\n", DEFAULT_RATE_LIMIT_COUNT);
    fprintf(file, "  \"defaultRateLimitSeconds\": %d,\n", DEFAULT_RATE_LIMIT_SECONDS);
    fprintf(file, "  \"filters\": [\n");
    
    for (int i = 0; i < filterCount; i++) {
        int count, seconds;
        getRateLimitValues(&perimeterFilters[i].rateLimiter, &count, &seconds);
        
        fprintf(file, "    {\n");
        fprintf(file, "      \"pattern\": \"%s\",\n", perimeterFilters[i].pattern);
        fprintf(file, "      \"enabled\": %s,\n", perimeterFilters[i].enabled ? "true" : "false");
        fprintf(file, "      \"triggerAction\": %s,\n", perimeterFilters[i].triggerAction ? "true" : "false");
        fprintf(file, "      \"action\": \"%s\",\n", perimeterFilters[i].action);
        fprintf(file, "      \"lastExecutionCount\": %d,\n", perimeterFilters[i].rateLimiter.lastExecutionCount);
        fprintf(file, "      \"lastExecutionTime\": %ld,\n", (long)perimeterFilters[i].rateLimiter.lastExecutionTime);
        fprintf(file, "      \"rateLimitCount\": %d,\n", count);
        fprintf(file, "      \"rateLimitSeconds\": %d\n", seconds);
        fprintf(file, "    }%s\n", (i < filterCount - 1) ? "," : "");
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    if (fclose(file) != 0) {
        perror("Error closing config file");
        return -1;
    }
    
    return 0;
}

int loadConfig(void) {
    if (!fileExists(CONFIG_FILE)) {
        printf("=== FIRST RUN SETUP ===\n");
        if (generateDefaultConfig() != 0) {
            printf("Failed to generate default config, starting with empty configuration\n");
            filterCount = 0;
            messagePrintingEnabled = 0;
            return -1;
        }
        printf("=== SETUP COMPLETE ===\n\n");
        return 0;
    }

    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        printf("Failed to open existing config file, generating default config\n");
        return generateDefaultConfig();
    }

    char line[1024];
    char pattern[MAX_PATTERN_LENGTH] = {0};
    char action[MAX_ACTION_LENGTH] = {0};
    int enabled = 1;
    int triggerAction = 0;
    int lastExecutionCount = 0;
    time_t lastExecutionTime = 0;
    int rateLimitCount = DEFAULT_RATE_LIMIT_COUNT;
    int rateLimitSeconds = DEFAULT_RATE_LIMIT_SECONDS;
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
        } else if (strstr(line, "\"rateLimitCount\":")) {
            sscanf(line, " \"rateLimitCount\": %d", &rateLimitCount);
        } else if (strstr(line, "\"rateLimitSeconds\":")) {
            sscanf(line, " \"rateLimitSeconds\": %d", &rateLimitSeconds);
        } else if (strstr(line, "}") && inFilter) {
            if (filterCount < MAX_FILTERS && pattern[0]) {
                strcpy(perimeterFilters[filterCount].pattern, pattern);
                perimeterFilters[filterCount].enabled = enabled;
                perimeterFilters[filterCount].triggerAction = triggerAction;
                strcpy(perimeterFilters[filterCount].action, action);
                perimeterFilters[filterCount].count = 0;
                perimeterFilters[filterCount].lastReceived = 0;
                
                initRateLimiterWithValues(&perimeterFilters[filterCount].rateLimiter, 
                                        rateLimitCount, rateLimitSeconds);
                perimeterFilters[filterCount].rateLimiter.lastExecutionCount = lastExecutionCount;
                perimeterFilters[filterCount].rateLimiter.lastExecutionTime = lastExecutionTime;
                
                filterCount++;
            }
            
            memset(pattern, 0, sizeof(pattern));
            memset(action, 0, sizeof(action));
            enabled = 1;
            triggerAction = 0;
            lastExecutionCount = 0;
            lastExecutionTime = 0;
            rateLimitCount = DEFAULT_RATE_LIMIT_COUNT;
            rateLimitSeconds = DEFAULT_RATE_LIMIT_SECONDS;
            inFilter = 0;
        }
    }
    
    fclose(file);
    printf("Loaded %d filters from config\n", filterCount);
    return 0;
}
