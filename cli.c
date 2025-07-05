#include "oscUtility.h"
#include "mediaControl.h"
#include "keyPress.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef void (*CommandFunc)(int argc, char args[][256]);

typedef struct {
    const char* name;
    CommandFunc func;
    int minArgs;
    const char* usage;
    const char* description;
} Command;

void cmd_help(int argc, char args[][256]) {
    (void)argc; (void)args; 
    printf("OSC Utility CLI Commands:\n");
    printf("  add <pattern>              - Add a new filter pattern\n");
    printf("  remove <pattern>           - Remove a filter pattern\n");
    printf("  list                       - List all filters\n");
    printf("  clear                      - Clear all filters\n");
    printf("  reset                      - Reset all filter counts\n");
    printf("  enable <pattern>           - Enable a filter\n");
    printf("  disable <pattern>          - Disable a filter\n");
    printf("  action <pattern> <command> - Set action command for filter\n");
    printf("  toggle <pattern>           - Toggle action execution for filter\n");
    printf("  rate <pattern> <count> <seconds> - Set rate limit for filter\n");
    printf("  rate-list                  - Show rate limiting settings\n");
    printf("  rate-reset <pattern>       - Reset filter rate limit to defaults\n");
    printf("  print                      - Toggle message printing on/off\n");
    printf("  status                     - Show system status\n");
    printf("  media-status               - Show current media player status\n");
    printf("  test-media                 - Test media controls\n");
    printf("  defaults                   - Setup default media control filters\n");
    printf("  show-defaults              - Show available default filters\n");
    printf("  actions                    - Show built-in actions\n");
    printf("  keys                       - List available keys for keypress actions\n");
    printf("  key-examples               - Show keypress action examples\n");
    printf("  test-key <keystring>       - Test a keypress action\n");
    printf("  save                       - Save current config\n");
    printf("  load                       - Reload config from file\n");
    printf("  hash-stats                 - Show key hash table performance stats\n");
    printf("  help                       - Show this help\n");
    printf("  exit                       - Exit CLI\n");
    printf("\nQuick Commands:\n");
    printf("  /                          - Toggle message printing (same as 'print')\n");
    printf("\nRate Limiting Examples:\n");
    printf("  rate discordmute 3 2       - Require 3 counts and 2 seconds\n");
    printf("  rate discordmute 1 0       - Execute on every message (no rate limit)\n");
    printf("\nNote: Message listening is always active in the background.\n");
    printf("      The '/' command only toggles whether messages are printed to console.\n");
}

void cmd_add(int argc, char args[][256]) {
    if (argc < 1) {
        printf("Usage: add <pattern>\n");
        return;
    }
    addPerimeterFilter(args[0]);
}

void cmd_remove(int argc, char args[][256]) {
    if (argc < 1) {
        printf("Usage: remove <pattern>\n");
        return;
    }
    removePerimeterFilter(args[0]);
}

void cmd_list(int argc, char args[][256]) {
    (void)argc; (void)args;
    listParameterFilters();
}

void cmd_clear(int argc, char args[][256]) {
    (void)argc; (void)args;
    clearParameterFilters();
}

void cmd_reset(int argc, char args[][256]) {
    (void)argc; (void)args;
    resetFilterCounts();
}

void cmd_enable(int argc, char args[][256]) {
    if (argc < 1) {
        printf("Usage: enable <pattern>\n");
        return;
    }
    enableFilter(args[0]);
}

void cmd_disable(int argc, char args[][256]) {
    if (argc < 1) {
        printf("Usage: disable <pattern>\n");
        return;
    }
    disableFilter(args[0]);
}

void cmd_action(int argc, char args[][256]) {
    if (argc < 2) {
        printf("Usage: action <pattern> <command>\n");
        return;
    }
    setFilterAction(args[0], args[1]);
}

void cmd_toggle(int argc, char args[][256]) {
    if (argc < 1) {
        printf("Usage: toggle <pattern>\n");
        return;
    }
    toggleFilterAction(args[0]);
}

void cmd_rate(int argc, char args[][256]) {
    if (argc < 3) {
        printf("Usage: rate <pattern> <count> <seconds>\n");
        printf("Examples:\n");
        printf("  rate discordmute 2 1     - Require 2 counts and 1 second\n");
        printf("  rate discordmute 1 0     - Execute immediately (no rate limit)\n");
        printf("  rate discordmute 5 3     - Require 5 counts and 3 seconds\n");
        return;
    }
    
    int count = atoi(args[1]);
    int seconds = atoi(args[2]);
    
    setFilterRateLimit(args[0], count, seconds);
}

void cmd_rate_list(int argc, char args[][256]) {
    (void)argc; (void)args;
    listFilterRateLimits();
}

void cmd_rate_reset(int argc, char args[][256]) {
    if (argc < 1) {
        printf("Usage: rate-reset <pattern>\n");
        return;
    }
    resetFilterRateLimit(args[0]);
}

void cmd_print(int argc, char args[][256]) {
    (void)argc; (void)args;
    toggleMessagePrinting();
}

void cmd_status(int argc, char args[][256]) {
    (void)argc; (void)args;
    printf("=== OSC Utility Status ===\n");
    printf("Message listening: ALWAYS ACTIVE (background)\n");
    printf("Message printing: %s\n", isMessagePrintingEnabled() ? "ENABLED" : "DISABLED");
    printf("Active filters: %d\n", filterCount);
    
    int activeFilters = 0;
    for (int i = 0; i < filterCount; i++) {
        if (perimeterFilters[i].count > 0) {
            activeFilters++;
        }
    }
    printf("Filters with matches: %d\n", activeFilters);
    printf("Default rate limiting: %d counts, %d seconds\n", 
           DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
}

void cmd_media_status(int argc, char args[][256]) {
    (void)argc; (void)args;
    updateMediaState();
    MediaState state = getMediaState();
    
    printf("=== Media Player Status ===\n");
    switch (state) {
        case MEDIA_STATE_PLAYING:
            printf("Status: PLAYING\n");
            break;
        case MEDIA_STATE_PAUSED:
            printf("Status: PAUSED\n");
            break;
        case MEDIA_STATE_STOPPED:
            printf("Status: STOPPED\n");
            break;
        default:
            printf("Status: UNKNOWN (no media player detected)\n");
            break;
    }
}

void cmd_test_media(int argc, char args[][256]) {
    (void)argc; (void)args;
    printf("Testing media controls...\n");
    printf("Play/Pause: ");
    mediaPlayPause();
    sleep(2);
    printf("Next: ");
    mediaNext();
    sleep(2);
    printf("Previous: ");
    mediaPrevious();
    printf("Media control test complete.\n");
}

void cmd_defaults(int argc, char args[][256]) {
    (void)argc; (void)args;
    setupDefaultFilters();
}

void cmd_show_defaults(int argc, char args[][256]) {
    (void)argc; (void)args;
    listDefaultFilters();
}

void cmd_actions(int argc, char args[][256]) {
    (void)argc; (void)args;
    listBuiltinActions();
}

void cmd_keys(int argc, char args[][256]) {
    (void)argc; (void)args;
    listAvailableKeys();
}

void cmd_key_examples(int argc, char args[][256]) {
    (void)argc; (void)args;
    listKeyPressExamples();
}

void cmd_test_key(int argc, char args[][256]) {
    if (argc < 1) {
        printf("Usage: test-key <keystring>\n");
        printf("Examples:\n");
        printf("  test-key a\n");
        printf("  test-key ctrl+c\n");
        printf("  test-key alt+tab\n");
        printf("  test-key hold:space:1000\n");
        return;
    }
    
    printf("Testing keypress: %s\n", args[0]);
    if (executeKeyPress(args[0])) {
        printf("Keypress executed successfully\n");
    } else {
        printf("Failed to execute keypress\n");
    }
}

void cmd_save(int argc, char args[][256]) {
    (void)argc; (void)args;
    if (saveConfig() == 0) {
        printf("Config saved successfully\n");
    }
}

void cmd_load(int argc, char args[][256]) {
    (void)argc; (void)args;
    if (loadConfig() == 0) {
        printf("Config loaded successfully\n");
    }
}

void cmd_exit(int argc, char args[][256]) {
    (void)argc; (void)args;
    printf("Goodbye!\n");
    exit(0);
}

void cmd_hash_stats(int argc, char args[][256]) {
    (void)argc; (void)args;
    printHashTableStats();
}

static const Command commands[] = {
    {"help",         cmd_help,         0, "help",                       "Show this help"},
    {"add",          cmd_add,          1, "add <pattern>",              "Add a new filter pattern"},
    {"remove",       cmd_remove,       1, "remove <pattern>",           "Remove a filter pattern"},
    {"list",         cmd_list,         0, "list",                       "List all filters"},
    {"clear",        cmd_clear,        0, "clear",                      "Clear all filters"},
    {"reset",        cmd_reset,        0, "reset",                      "Reset all filter counts"},
    {"enable",       cmd_enable,       1, "enable <pattern>",           "Enable a filter"},
    {"disable",      cmd_disable,      1, "disable <pattern>",          "Disable a filter"},
    {"action",       cmd_action,       2, "action <pattern> <command>", "Set action command for filter"},
    {"toggle",       cmd_toggle,       1, "toggle <pattern>",           "Toggle action execution for filter"},
    {"rate",         cmd_rate,         3, "rate <pattern> <count> <seconds>", "Set rate limit for filter"},
    {"rate-list",    cmd_rate_list,    0, "rate-list",                  "Show rate limiting settings"},
    {"rate-reset",   cmd_rate_reset,   1, "rate-reset <pattern>",       "Reset filter rate limit to defaults"},
    {"print",        cmd_print,        0, "print",                      "Toggle message printing"},
    {"status",       cmd_status,       0, "status",                     "Show system status"},
    {"media-status", cmd_media_status, 0, "media-status",               "Show media player status"},
    {"test-media",   cmd_test_media,   0, "test-media",                 "Test media controls"},
    {"defaults",     cmd_defaults,     0, "defaults",                   "Setup default media control filters"},
    {"show-defaults", cmd_show_defaults, 0, "show-defaults",             "Show available default filters"},
    {"actions",      cmd_actions,      0, "actions",                    "Show built-in actions"},
    {"keys",         cmd_keys,         0, "keys",                       "List available keys"},
    {"key-examples", cmd_key_examples, 0, "key-examples",               "Show keypress examples"},
    {"test-key",     cmd_test_key,     1, "test-key <keystring>",       "Test a keypress action"},
    {"save",         cmd_save,         0, "save",                       "Save current config"},
    {"load",         cmd_load,         0, "load",                       "Reload config from file"},
    {"exit",         cmd_exit,         0, "exit",                       "Exit CLI"},
    {"hash-stats",   cmd_hash_stats,   0, "hash-stats",                 "Show key hash table statistics"},
    {NULL,           NULL,             0, NULL,                         NULL} 
};

const Command* findCommand(const char* name) {
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

void runCLI(void) {
    char input[1024];
    char args[10][256]; 
    int argc;
    
    printf("=== OSC Utility CLI ===\n");
    printf("Listening for messages in background...\n");
    printf("Message printing is %s (press '/' to toggle)\n", 
           isMessagePrintingEnabled() ? "ENABLED" : "DISABLED");
    printf("Type 'help' for commands, 'defaults' to setup media controls\n");
    printf("Default rate limiting: %d counts, %d seconds\n\n", 
           DEFAULT_RATE_LIMIT_COUNT, DEFAULT_RATE_LIMIT_SECONDS);
    
    while (1) {
        printf("osc%s> ", isMessagePrintingEnabled() ? "" : " [QUIET]");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;
        }
        
        if (strcmp(input, "/") == 0) {
            toggleMessagePrinting();
            continue;
        }
        
        argc = 0;
        char* token = strtok(input, " \t");
        while (token != NULL && argc < 10) {
            strncpy(args[argc], token, 255);
            args[argc][255] = '\0';
            argc++;
            token = strtok(NULL, " \t");
        }
        
        if (argc == 0) {
            continue;
        }
        
        const Command* cmd = findCommand(args[0]);
        if (cmd != NULL) {
            if (argc - 1 < cmd->minArgs) {
                printf("Usage: %s\n", cmd->usage);
            } else {
                cmd->func(argc - 1, &args[1]);
            }
        } else {
            printf("Unknown command: %s\n", args[0]);
            printf("Type 'help' for available commands\n");
        }
    }
}
